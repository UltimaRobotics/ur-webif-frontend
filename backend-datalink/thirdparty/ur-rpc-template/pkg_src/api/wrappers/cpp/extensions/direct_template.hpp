#ifndef DIRECT_TEMPLATE_HPP
#define DIRECT_TEMPLATE_HPP

#include "direct_template.h"
#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <map>
#include <random>
#include <csignal>
#include <cJSON.h>

namespace DirectTemplate {

// Forward declarations
class ClientThread;
class AsyncRPCCall;
class TargetedRPCRequester;
class TargetedRPCResponder;

// Type aliases for better readability
using MessageHandler = std::function<void(const std::string& topic, const std::string& payload)>;
using ResponseHandler = std::function<void(bool success, const std::string& result, const std::string& error_message, int error_code)>;
using ConnectionStatusCallback = std::function<void(bool connected, const std::string& reason)>;

// Exception classes
class DirectTemplateException : public std::exception {
private:
    std::string message_;
public:
    explicit DirectTemplateException(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }
};

class ConnectionException : public DirectTemplateException {
public:
    explicit ConnectionException(const std::string& message) : DirectTemplateException("Connection error: " + message) {}
};

class ConfigException : public DirectTemplateException {
public:
    explicit ConfigException(const std::string& message) : DirectTemplateException("Configuration error: " + message) {}
};

// Statistics structure
struct Statistics {
    uint64_t messagesSent;
    uint64_t messagesReceived;
    uint64_t requestsSent;
    uint64_t responsesReceived;
    uint64_t errorsCount;
    uint64_t uptimeSeconds;
    time_t lastActivity;
    bool isConnected;

    Statistics() : messagesSent(0), messagesReceived(0), requestsSent(0),
                  responsesReceived(0), errorsCount(0), uptimeSeconds(0),
                  lastActivity(0), isConnected(false) {}
};

// Reconnection configuration
struct ReconnectConfig {
    int maxAttempts;
    int delayMs;
    bool enabled;

    ReconnectConfig(int max_attempts = 5, int delay_ms = 5000, bool enable = true)
        : maxAttempts(max_attempts), delayMs(delay_ms), enabled(enable) {}
};

// Global Client Manager (Singleton)
class GlobalClient {
private:
    static std::unique_ptr<GlobalClient> instance_;
    static std::mutex instance_mutex_;

    std::mutex client_mutex_;
    std::atomic<bool> initialized_;

    GlobalClient() : client_(nullptr), initialized_(false) {}

public:
    ur_rpc_client_t* client_;
    static GlobalClient& getInstance();

    bool initialize(const std::string& configPath);
    void cleanup();
    bool isInitialized() const { return initialized_.load(); }

    // Async RPC operations
    void sendAsyncRPC(const std::string& method, const std::string& service,
                     const std::string& params = "",
                     int authority = UR_RPC_AUTHORITY_USER);

    void sendAsyncRPCWithCallback(const std::string& method, const std::string& service,
                                 const std::string& params, const ResponseHandler& callback,
                                 int authority = UR_RPC_AUTHORITY_USER);

    void sendNotification(const std::string& method, const std::string& service,
                         const std::string& params = "",
                         int authority = UR_RPC_AUTHORITY_USER);

    void publishRawMessage(const std::string& topic, const std::string& payload);

    // Topic management
    void subscribeTopic(const std::string& topic);
    void unsubscribeTopic(const std::string& topic);

    // Status and statistics
    bool isConnected() const;
    Statistics getStatistics() const;
    std::string getStatusString() const;

    ~GlobalClient();
};

// Async RPC Call wrapper
class AsyncRPCCall {
private:
    std::string method_;
    std::string service_;
    std::string params_;
    int authority_;
    ResponseHandler callback_;

public:
    AsyncRPCCall(const std::string& method, const std::string& service,
                const std::string& params = "", int authority = UR_RPC_AUTHORITY_USER)
        : method_(method), service_(service), params_(params), authority_(authority) {}

    AsyncRPCCall& setCallback(const ResponseHandler& callback) {
        callback_ = callback;
        return *this;
    }

    AsyncRPCCall& setAuthority(int authority) {
        authority_ = authority;
        return *this;
    }

    AsyncRPCCall& setParams(const std::string& params) {
        params_ = params;
        return *this;
    }

    void execute();
    void executeWithGlobalClient();
};

// Client Thread Manager
class ClientThread {
private:
    std::atomic<bool> running_;
    std::mutex status_mutex_;
    MessageHandler message_handler_;
    ConnectionStatusCallback connection_callback_;
    ReconnectConfig reconnect_config_;

    static void staticMessageHandler(const char* topic, const char* payload, size_t payload_len, void* user_data);

public:
    explicit ClientThread(const std::string& configPath);
    ~ClientThread();
    direct_client_thread_t* thread_ctx_;

    // Thread control
    bool start();
    bool stop();
    bool isRunning() const { return running_.load(); }
    bool isConnected() const;
    bool waitForConnection(int timeout_ms);

    // Configuration
    void setReconnectConfig(const ReconnectConfig& config);
    void setMessageHandler(const MessageHandler& handler);
    void setConnectionStatusCallback(const ConnectionStatusCallback& callback);

    // Operations
    void triggerReconnect();
    Statistics getStatistics() const;
    void printStatistics() const;

    // Topic management
    void subscribeTopic(const std::string& topic);
    void unsubscribeTopic(const std::string& topic);

    // Heartbeat control
    bool startHeartbeat();
    bool stopHeartbeat();

    // Async operations (uses the thread's client)
    void sendAsyncRPC(const std::string& method, const std::string& service,
                     const std::string& params = "", int authority = UR_RPC_AUTHORITY_USER);

    void sendNotification(const std::string& method, const std::string& service,
                         const std::string& params = "", int authority = UR_RPC_AUTHORITY_USER);

    void publishRawMessage(const std::string& topic, const std::string& payload);
};

// Utility functions
class Utils {
public:
    static std::string parseJSONString(const std::string& json, const std::string& key);
    static std::string createJSONParams(const std::map<std::string, std::string>& params);
    static void logInfo(const std::string& message);
    static void logError(const std::string& message);
    static std::string getCurrentTimestamp();

    // Template function for async RPC calls (similar to the provided example)
    template<typename ParamsType>
    static void performAsyncRPCCall(const std::string& method, const std::string& service,
                                   const ParamsType& params, const ResponseHandler& callback) {
        try {
            GlobalClient& client = GlobalClient::getInstance();
            if (!client.isConnected()) {
                throw ConnectionException("Client not connected");
            }

            std::string paramsStr;
            if constexpr (std::is_same_v<ParamsType, std::string>) {
                paramsStr = params;
            } else {
                // Assume params can be converted to string
                paramsStr = std::string(params);
            }

            client.sendAsyncRPCWithCallback(method, service, paramsStr, callback);

        } catch (const DirectTemplateException& e) {
            logError("Failed to send RPC request: " + std::string(e.what()));
            if (callback) {
                callback(false, "", e.what(), -1);
            }
        } catch (const std::exception& e) {
            logError("Unexpected error while sending RPC request: " + std::string(e.what()));
            if (callback) {
                callback(false, "", e.what(), -1);
            }
        }
    }
};

// Default message handler (weak symbol equivalent)
extern MessageHandler g_default_message_handler;
void setDefaultMessageHandler(const MessageHandler& handler);

// Generic message handler function that can be used by both requester and responder
void handleIncomingMessage(const std::string& topic, const std::string& payload) __attribute__((weak));
// Specialized message handler function for targeted messaging
void handleTargetedMessage(const std::string& topic, const std::string& payload,
                          TargetedRPCRequester* requester = nullptr,
                          TargetedRPCResponder* responder = nullptr);

// Template for creating RPC client threads
template<typename ConfigType = std::string>
class RPCClientTemplate {
private:
    std::unique_ptr<ClientThread> client_thread_;
    std::atomic<bool> auto_reconnect_;

public:
    explicit RPCClientTemplate(const ConfigType& config) : auto_reconnect_(true) {
        if constexpr (std::is_same_v<ConfigType, std::string>) {
            client_thread_ = std::make_unique<ClientThread>(config);
        } else {
            // Handle other config types
            client_thread_ = std::make_unique<ClientThread>(std::string(config));
        }

        // Set up automatic reconnection
        client_thread_->setConnectionStatusCallback([this](bool connected, const std::string& reason) {
            if (!connected && auto_reconnect_.load()) {
                Utils::logInfo("Connection lost: " + reason + ". Attempting reconnection...");
                std::this_thread::sleep_for(std::chrono::seconds(2));
                client_thread_->triggerReconnect();
            }
        });
    }

    ~RPCClientTemplate() {
        if (client_thread_ && client_thread_->isRunning()) {
            client_thread_->stop();
        }
    }

    bool start() { return client_thread_->start(); }
    bool stop() { return client_thread_->stop(); }
    bool isRunning() const { return client_thread_->isRunning(); }
    bool isConnected() const { return client_thread_->isConnected(); }

    void setAutoReconnect(bool enable) { auto_reconnect_.store(enable); }
    void setMessageHandler(const MessageHandler& handler) { client_thread_->setMessageHandler(handler); }

    ClientThread& getClientThread() { return *client_thread_; }
    const ClientThread& getClientThread() const { return *client_thread_; }
};

// Targeted RPC Messaging Classes
class TargetedRPCRequester {
private:
    std::unique_ptr<ClientThread> client_;
    std::atomic<bool> active_;
    std::atomic<int> request_counter_;

    struct PendingRequest {
        std::string transaction_id;
        std::string target_client;
        std::chrono::steady_clock::time_point sent_time;
        std::string method;
        std::string response_topic;
        ResponseHandler callback;
    };

    std::map<std::string, PendingRequest> pending_requests_;
    std::mutex pending_mutex_;
    std::mt19937 rng_;

public:
    explicit TargetedRPCRequester(ClientThread* client_thread);
    ~TargetedRPCRequester();

    // Send targeted request with callback
    void sendTargetedRequest(const std::string& target_client, const std::string& method,
                           const std::string& data, const ResponseHandler& callback = nullptr);

    // Handle response message on unique topic
    void handleResponseMessage(const std::string& topic, const std::string& payload);

    // Check if requester is active
    bool isActive() const { return active_.load(); }

    // Get pending request count
    size_t getPendingRequestCount();

private:
    void setupResponseHandler();
    std::string generateTransactionId(const std::string& target_client, const std::string& method);
    std::string generateUniqueResponseTopic(const std::string& transaction_id);
    cJSON* createRequestJson(const std::string& target_client, const std::string& method,
                           const std::string& data, const std::string& transaction_id,
                           const std::string& response_topic, int req_num);
};

class TargetedRPCResponder {
private:
    std::unique_ptr<ClientThread> client_;
    std::atomic<bool> active_;
    std::atomic<int> response_counter_;
    std::string client_id_;
    std::mt19937 rng_;

    std::function<bool(const std::string&, const std::string&)> request_processor_;

public:
    explicit TargetedRPCResponder(ClientThread* client_thread, const std::string& client_id);
    ~TargetedRPCResponder();

    // Set custom request processor
    void setRequestProcessor(const std::function<bool(const std::string&, const std::string&)>& processor) ;

    // Handle incoming request message
    void handleRequestMessage(const std::string& topic, const std::string& payload)__attribute__((weak));

    // Check if responder is active
    bool isActive() const { return active_.load(); }

    // Get response count
    int getResponseCount() const { return response_counter_.load(); }

private:
    void setupRequestHandler();
    std::string extractJsonValue(const std::string& json, const std::string& key);
    __attribute__((weak)) bool processRequest(const std::string& method, const std::string& payload);
    void sendResponse(const std::string& response_topic, const std::string& transaction_id,
                     const std::string& method, bool success, const std::string& message,
                     int processing_time_ms);
    cJSON* createResponseJson(const std::string& transaction_id, const std::string& method,
                            bool success, const std::string& message, int processing_time_ms);
};

// Factory functions for easy creation
std::unique_ptr<TargetedRPCRequester> createTargetedRequester(ClientThread* client_thread);
std::unique_ptr<TargetedRPCResponder> createTargetedResponder(ClientThread* client_thread, const std::string& client_id);

// Convenience aliases
using SimpleRPCClient = RPCClientTemplate<std::string>;

} // namespace DirectTemplate

#endif /* DIRECT_TEMPLATE_HPP */