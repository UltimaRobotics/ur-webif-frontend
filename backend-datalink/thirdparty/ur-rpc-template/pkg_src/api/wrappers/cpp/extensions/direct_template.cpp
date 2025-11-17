
#include "direct_template.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <random>
#include <cJSON.h>

namespace DirectTemplate {

// Static member definitions
std::unique_ptr<GlobalClient> GlobalClient::instance_;
std::mutex GlobalClient::instance_mutex_;

// Default message handler
MessageHandler g_default_message_handler = [](const std::string& topic, const std::string& payload) {
    std::cout << "[DEFAULT_HANDLER] Topic: " << topic << std::endl;
    std::cout << "[DEFAULT_HANDLER] Payload: " << payload << std::endl;
    std::cout << "[DEFAULT_HANDLER] Override setDefaultMessageHandler() to implement custom handling" << std::endl;
};

void setDefaultMessageHandler(const MessageHandler& handler) {
    g_default_message_handler = handler;
}

// C++ wrapper for the weak symbol handle_data function
extern "C" void handle_data(const char* topic, const char* payload, size_t payload_len) {
    if (g_default_message_handler) {
        std::string topic_str(topic ? topic : "");
        std::string payload_str(payload ? std::string(payload, payload_len) : "");
        g_default_message_handler(topic_str, payload_str);
    }
}

// GlobalClient implementation
GlobalClient& GlobalClient::getInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<GlobalClient>(new GlobalClient());
    }
    return *instance_;
}

bool GlobalClient::initialize(const std::string& configPath) {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (initialized_.load()) {
        return true; // Already initialized
    }
    
    int result = direct_client_init_global(configPath.c_str());
    if (result == UR_RPC_SUCCESS) {
        client_ = direct_client_get_global();
        initialized_.store(true);
        return true;
    }
    
    return false;
}

void GlobalClient::cleanup() {
    std::lock_guard<std::mutex> lock(client_mutex_);
    
    if (initialized_.load()) {
        direct_client_cleanup_global();
        client_ = nullptr;
        initialized_.store(false);
    }
}

void GlobalClient::sendAsyncRPC(const std::string& method, const std::string& service, 
                               const std::string& params, int authority) {
    if (!initialized_.load()) {
        throw ConnectionException("Global client not initialized");
    }
    
    cJSON* params_json = nullptr;
    if (!params.empty()) {
        params_json = cJSON_Parse(params.c_str());
        if (!params_json) {
            throw ConfigException("Invalid JSON parameters: " + params);
        }
    }
    
    int result = direct_client_send_async_rpc(method.c_str(), service.c_str(), 
                                             params_json, static_cast<ur_rpc_authority_t>(authority));
    
    if (params_json) {
        cJSON_Delete(params_json);
    }
    
    if (result != UR_RPC_SUCCESS) {
        throw DirectTemplateException("Failed to send async RPC: " + std::string(ur_rpc_error_string(static_cast<ur_rpc_error_t>(result))));
    }
}

void GlobalClient::sendAsyncRPCWithCallback(const std::string& method, const std::string& service,
                                           const std::string& params, const ResponseHandler& callback,
                                           int authority) {
    if (!initialized_.load()) {
        throw ConnectionException("Global client not initialized");
    }
    
    cJSON* params_json = nullptr;
    if (!params.empty()) {
        params_json = cJSON_Parse(params.c_str());
        if (!params_json) {
            throw ConfigException("Invalid JSON parameters: " + params);
        }
    }
    
    // Create a callback wrapper
    auto* callback_ptr = new ResponseHandler(callback);
    
    auto c_callback = [](const ur_rpc_response_t* response, void* user_data) {
        auto* cpp_callback = static_cast<ResponseHandler*>(user_data);
        if (cpp_callback && response) {
            std::string result_str;
            if (response->result) {
                char* result_json = cJSON_Print(response->result);
                if (result_json) {
                    result_str = result_json;
                    free(result_json);
                }
            }
            
            std::string error_msg = response->error_message ? response->error_message : "";
            (*cpp_callback)(response->success, result_str, error_msg, response->error_code);
        }
        delete cpp_callback;
    };
    
    int result = direct_client_send_async_rpc_with_callback(method.c_str(), service.c_str(), 
                                                           params_json, static_cast<ur_rpc_authority_t>(authority),
                                                           c_callback, callback_ptr);
    
    if (params_json) {
        cJSON_Delete(params_json);
    }
    
    if (result != UR_RPC_SUCCESS) {
        delete callback_ptr;
        throw DirectTemplateException("Failed to send async RPC with callback: " + std::string(ur_rpc_error_string(static_cast<ur_rpc_error_t>(result))));
    }
}

void GlobalClient::sendNotification(const std::string& method, const std::string& service,
                                   const std::string& params, int authority) {
    if (!initialized_.load()) {
        throw ConnectionException("Global client not initialized");
    }
    
    cJSON* params_json = nullptr;
    if (!params.empty()) {
        params_json = cJSON_Parse(params.c_str());
        if (!params_json) {
            throw ConfigException("Invalid JSON parameters: " + params);
        }
    }
    
    int result = direct_client_send_notification(method.c_str(), service.c_str(), 
                                                params_json, static_cast<ur_rpc_authority_t>(authority));
    
    if (params_json) {
        cJSON_Delete(params_json);
    }
    
    if (result != UR_RPC_SUCCESS) {
        throw DirectTemplateException("Failed to send notification: " + std::string(ur_rpc_error_string(static_cast<ur_rpc_error_t>(result))));
    }
}

void GlobalClient::publishRawMessage(const std::string& topic, const std::string& payload) {
    if (!initialized_.load()) {
        throw ConnectionException("Global client not initialized");
    }
    
    int result = direct_client_publish_raw_message(topic.c_str(), payload.c_str(), payload.length());
    
    if (result != UR_RPC_SUCCESS) {
        throw DirectTemplateException("Failed to publish raw message: " + std::string(ur_rpc_error_string(static_cast<ur_rpc_error_t>(result))));
    }
}

void GlobalClient::subscribeTopic(const std::string& topic) {
    if (!initialized_.load()) {
        throw ConnectionException("Global client not initialized");
    }
    
    int result = direct_client_subscribe_topic(topic.c_str());
    
    if (result != UR_RPC_SUCCESS) {
        throw DirectTemplateException("Failed to subscribe to topic: " + std::string(ur_rpc_error_string(static_cast<ur_rpc_error_t>(result))));
    }
}

void GlobalClient::unsubscribeTopic(const std::string& topic) {
    if (!initialized_.load()) {
        throw ConnectionException("Global client not initialized");
    }
    
    int result = direct_client_unsubscribe_topic(topic.c_str());
    
    if (result != UR_RPC_SUCCESS) {
        throw DirectTemplateException("Failed to unsubscribe from topic: " + std::string(ur_rpc_error_string(static_cast<ur_rpc_error_t>(result))));
    }
}

bool GlobalClient::isConnected() const {
    if (!initialized_.load()) {
        return false;
    }
    
    direct_client_statistics_t stats;
    if (direct_client_get_statistics(&stats) == UR_RPC_SUCCESS) {
        return stats.is_connected;
    }
    
    return false;
}

Statistics GlobalClient::getStatistics() const {
    Statistics stats;
    
    if (!initialized_.load()) {
        return stats;
    }
    
    direct_client_statistics_t c_stats;
    if (direct_client_get_statistics(&c_stats) == UR_RPC_SUCCESS) {
        stats.messagesSent = c_stats.messages_sent;
        stats.messagesReceived = c_stats.messages_received;
        stats.requestsSent = c_stats.requests_sent;
        stats.responsesReceived = c_stats.responses_received;
        stats.errorsCount = c_stats.errors_count;
        stats.uptimeSeconds = c_stats.uptime_seconds;
        stats.lastActivity = c_stats.last_activity;
        stats.isConnected = c_stats.is_connected;
    }
    
    return stats;
}

std::string GlobalClient::getStatusString() const {
    if (!initialized_.load()) {
        return "Not initialized";
    }
    
    return isConnected() ? "Connected" : "Disconnected";
}

GlobalClient::~GlobalClient() {
    cleanup();
}

// AsyncRPCCall implementation
void AsyncRPCCall::execute() {
    if (callback_) {
        GlobalClient::getInstance().sendAsyncRPCWithCallback(method_, service_, params_, callback_, authority_);
    } else {
        GlobalClient::getInstance().sendAsyncRPC(method_, service_, params_, authority_);
    }
}

void AsyncRPCCall::executeWithGlobalClient() {
    execute(); // Same as execute() since we're using the global client
}

// ClientThread implementation
ClientThread::ClientThread(const std::string& configPath) 
    : thread_ctx_(nullptr), running_(false) {
    thread_ctx_ = direct_client_thread_create(configPath.c_str());
    if (!thread_ctx_) {
        throw DirectTemplateException("Failed to create client thread context");
    }
    
    // Initialize the global client for publishing messages
    std::cout << "[DIRECT_TEMPLATE] Initializing GlobalClient..." << std::endl;
    if (!GlobalClient::getInstance().initialize(configPath)) {
        direct_client_thread_destroy(thread_ctx_);
        thread_ctx_ = nullptr;
        throw DirectTemplateException("Failed to initialize GlobalClient");
    }
    std::cout << "[DIRECT_TEMPLATE] GlobalClient initialized successfully" << std::endl;
}

ClientThread::~ClientThread() {
    if (running_.load()) {
        stop();
    }
    if (thread_ctx_) {
        direct_client_thread_destroy(thread_ctx_);
    }
}

bool ClientThread::start() {
    if (running_.load()) {
        return true;
    }
    
    int result = direct_client_thread_start(thread_ctx_);
    if (result == UR_RPC_SUCCESS) {
        running_.store(true);
        return true;
    }
    
    return false;
}

bool ClientThread::stop() {
    if (!running_.load()) {
        return true;
    }
    
    int result = direct_client_thread_stop(thread_ctx_);
    if (result == UR_RPC_SUCCESS) {
        running_.store(false);
        return true;
    }
    
    return false;
}

bool ClientThread::isConnected() const {
    if (!thread_ctx_) {
        return false;
    }
    
    return direct_client_thread_is_connected(thread_ctx_);
}

bool ClientThread::waitForConnection(int timeout_ms) {
    if (!thread_ctx_) {
        return false;
    }
    
    return direct_client_thread_wait_for_connection(thread_ctx_, timeout_ms);
}

void ClientThread::setReconnectConfig(const ReconnectConfig& config) {
    reconnect_config_ = config;
    if (thread_ctx_) {
        direct_client_set_reconnect_params(thread_ctx_, config.maxAttempts, config.delayMs);
    }
}

void ClientThread::setMessageHandler(const MessageHandler& handler) {
    message_handler_ = handler;
    // Update the global default handler
    setDefaultMessageHandler(handler);
}

void ClientThread::setConnectionStatusCallback(const ConnectionStatusCallback& callback) {
    connection_callback_ = callback;
}

void ClientThread::triggerReconnect() {
    if (thread_ctx_) {
        direct_client_trigger_reconnect(thread_ctx_);
    }
}

Statistics ClientThread::getStatistics() const {
    return GlobalClient::getInstance().getStatistics();
}

void ClientThread::printStatistics() const {
    auto stats = getStatistics();
    
    std::cout << "=== Client Thread Statistics ===" << std::endl;
    std::cout << "Messages sent: " << stats.messagesSent << std::endl;
    std::cout << "Messages received: " << stats.messagesReceived << std::endl;
    std::cout << "Requests sent: " << stats.requestsSent << std::endl;
    std::cout << "Responses received: " << stats.responsesReceived << std::endl;
    std::cout << "Errors: " << stats.errorsCount << std::endl;
    std::cout << "Uptime: " << stats.uptimeSeconds << " seconds" << std::endl;
    std::cout << "Connected: " << (stats.isConnected ? "Yes" : "No") << std::endl;
    std::cout << "===============================" << std::endl;
}

void ClientThread::subscribeTopic(const std::string& topic) {
    GlobalClient::getInstance().subscribeTopic(topic);
}

void ClientThread::unsubscribeTopic(const std::string& topic) {
    GlobalClient::getInstance().unsubscribeTopic(topic);
}

bool ClientThread::startHeartbeat() {
    if (!thread_ctx_) {
        return false;
    }
    
    return direct_client_start_heartbeat(thread_ctx_) == UR_RPC_SUCCESS;
}

bool ClientThread::stopHeartbeat() {
    if (!thread_ctx_) {
        return false;
    }
    
    return direct_client_stop_heartbeat(thread_ctx_) == UR_RPC_SUCCESS;
}

void ClientThread::sendAsyncRPC(const std::string& method, const std::string& service,
                               const std::string& params, int authority) {
    GlobalClient::getInstance().sendAsyncRPC(method, service, params, authority);
}

void ClientThread::sendNotification(const std::string& method, const std::string& service,
                                   const std::string& params, int authority) {
    GlobalClient::getInstance().sendNotification(method, service, params, authority);
}

void ClientThread::publishRawMessage(const std::string& topic, const std::string& payload) {
    GlobalClient::getInstance().publishRawMessage(topic, payload);
}

void ClientThread::staticMessageHandler(const char* topic, const char* payload, size_t payload_len, void* user_data) {
    auto* client_thread = static_cast<ClientThread*>(user_data);
    if (client_thread && client_thread->message_handler_) {
        std::string topic_str(topic ? topic : "");
        std::string payload_str(payload ? std::string(payload, payload_len) : "");
        client_thread->message_handler_(topic_str, payload_str);
    }
}

// Utils implementation
std::string Utils::parseJSONString(const std::string& json, const std::string& key) {
    cJSON* root = cJSON_Parse(json.c_str());
    if (!root) {
        return "";
    }
    
    cJSON* item = cJSON_GetObjectItem(root, key.c_str());
    std::string result;
    if (cJSON_IsString(item)) {
        result = item->valuestring;
    }
    
    cJSON_Delete(root);
    return result;
}

std::string Utils::createJSONParams(const std::map<std::string, std::string>& params) {
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return "{}";
    }
    
    for (const auto& pair : params) {
        cJSON_AddStringToObject(root, pair.first.c_str(), pair.second.c_str());
    }
    
    char* json_str = cJSON_Print(root);
    std::string result = json_str ? json_str : "{}";
    
    if (json_str) {
        free(json_str);
    }
    cJSON_Delete(root);
    
    return result;
}

void Utils::logInfo(const std::string& message) {
#ifdef _DEBUG_LOG
    std::cout << "[DIRECT_TEMPLATE_INFO] " << getCurrentTimestamp() << " " << message << std::endl;    
#endif
}

void Utils::logError(const std::string& message) {
    std::cerr << "[DIRECT_TEMPLATE_ERROR] " << getCurrentTimestamp() << " " << message << std::endl;
}

std::string Utils::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

// TargetedRPCRequester implementation
TargetedRPCRequester::TargetedRPCRequester(ClientThread* client_thread) 
    : client_(nullptr), active_(false), request_counter_(0), 
      rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    
    if (!client_thread) {
        throw DirectTemplateException("Client thread cannot be null");
    }
    
    // We don't own the client thread, just use it
    client_ = std::unique_ptr<ClientThread>(client_thread);
    setupResponseHandler();
    active_.store(true);
}

TargetedRPCRequester::~TargetedRPCRequester() {
    active_.store(false);
    // Release the client thread without deleting it
    client_.release();
}

void TargetedRPCRequester::setupResponseHandler() {
    if (!client_) return;
    
    // Response handler will subscribe to unique topics per request
    Utils::logInfo("TargetedRPCRequester response handler initialized");
}

std::string TargetedRPCRequester::generateTransactionId(const std::string& target_client, const std::string& method) {
    int req_num = ++request_counter_;
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return "req_" + target_client + "_" + method + "_" + std::to_string(req_num) + "_" + std::to_string(now);
}

std::string TargetedRPCRequester::generateUniqueResponseTopic(const std::string& transaction_id) {
    return "direct_messaging/responses/" + transaction_id;
}

cJSON* TargetedRPCRequester::createRequestJson(const std::string& target_client, const std::string& method, 
                                              const std::string& data, const std::string& transaction_id, 
                                              const std::string& response_topic, int req_num) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    cJSON* request_json = cJSON_CreateObject();
    cJSON* params = cJSON_CreateObject();
    
    cJSON_AddStringToObject(request_json, "type", "targeted_request");
    cJSON_AddStringToObject(request_json, "sender", "requester_client");
    cJSON_AddStringToObject(request_json, "target", target_client.c_str());
    cJSON_AddStringToObject(request_json, "method", method.c_str());
    cJSON_AddStringToObject(request_json, "transaction_id", transaction_id.c_str());
    cJSON_AddStringToObject(request_json, "response_topic", response_topic.c_str());
    cJSON_AddNumberToObject(request_json, "timestamp", now);
    cJSON_AddNumberToObject(request_json, "request_number", req_num);
    
    cJSON_AddStringToObject(params, "data", data.c_str());
    cJSON_AddStringToObject(params, "priority", "normal");
    cJSON_AddItemToObject(request_json, "params", params);
    
    return request_json;
}

void TargetedRPCRequester::sendTargetedRequest(const std::string& target_client, const std::string& method, 
                                              const std::string& data, const ResponseHandler& callback) {
    if (!active_.load() || !client_) {
        throw DirectTemplateException("Requester is not active");
    }
    
    std::string transaction_id = generateTransactionId(target_client, method);
    std::string response_topic = generateUniqueResponseTopic(transaction_id);
    int req_num = request_counter_.load();
    
    // Subscribe to unique response topic before sending request
    try {
        client_->subscribeTopic(response_topic);
        Utils::logInfo("Subscribed to response topic: " + response_topic);
    } catch (const std::exception& e) {
        throw DirectTemplateException("Failed to subscribe to response topic: " + std::string(e.what()));
    }
    
    // Create request JSON using cJSON
    cJSON* request_json = createRequestJson(target_client, method, data, transaction_id, response_topic, req_num);
    
    char* json_string = cJSON_Print(request_json);
    if (!json_string) {
        cJSON_Delete(request_json);
        throw DirectTemplateException("Failed to create JSON string");
    }
    
    // Create topic for specific client
    std::string topic = "direct_messaging/" + target_client + "/requests";
    
    Utils::logInfo("Sending targeted request #" + std::to_string(req_num) + " to " + target_client);
    
    try {
        client_->publishRawMessage(topic, json_string);
        
        // Store pending request with response topic
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_requests_[transaction_id] = {
            transaction_id, target_client, std::chrono::steady_clock::now(), method, response_topic, callback
        };
        
        Utils::logInfo("Request #" + std::to_string(req_num) + " sent to " + target_client + " successfully");
        
    } catch (const std::exception& e) {
        free(json_string);
        cJSON_Delete(request_json);
        // Unsubscribe on failure
        try {
            client_->unsubscribeTopic(response_topic);
        } catch (...) {}
        throw DirectTemplateException("Failed to send request: " + std::string(e.what()));
    }
    
    free(json_string);
    cJSON_Delete(request_json);
}

void TargetedRPCRequester::handleResponseMessage(const std::string& topic, const std::string& payload) {
    if (!active_.load() || topic.find("direct_messaging/responses/") == std::string::npos) {
        return;
    }
    
    try {
        // Parse JSON using cJSON
        cJSON* response_json = cJSON_Parse(payload.c_str());
        if (!response_json) {
            Utils::logError("Failed to parse response JSON");
            return;
        }
        
        cJSON* txn_item = cJSON_GetObjectItem(response_json, "transaction_id");
        cJSON* success_item = cJSON_GetObjectItem(response_json, "success");
        cJSON* message_item = cJSON_GetObjectItem(response_json, "message");
        cJSON* processing_time_item = cJSON_GetObjectItem(response_json, "processing_time_ms");
        
        if (!cJSON_IsString(txn_item) || !cJSON_IsBool(success_item)) {
            cJSON_Delete(response_json);
            Utils::logError("Invalid response JSON format");
            return;
        }
        
        std::string transaction_id = txn_item->valuestring;
        bool success = cJSON_IsTrue(success_item);
        std::string message = cJSON_IsString(message_item) ? message_item->valuestring : "";
        int processing_time = cJSON_IsNumber(processing_time_item) ? processing_time_item->valueint : 0;
        
        std::lock_guard<std::mutex> lock(pending_mutex_);
        auto it = pending_requests_.find(transaction_id);
        if (it != pending_requests_.end()) {
            auto duration = std::chrono::steady_clock::now() - it->second.sent_time;
            auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
            
            Utils::logInfo("Response received for transaction: " + transaction_id);
            
            // Call callback if provided
            if (it->second.callback) {
                it->second.callback(success, message, success ? "" : "Processing error", success ? 0 : -1);
            }
            
            // Unsubscribe from unique response topic
            try {
                client_->unsubscribeTopic(it->second.response_topic);
                Utils::logInfo("Unsubscribed from response topic: " + it->second.response_topic);
            } catch (const std::exception& e) {
                Utils::logError("Failed to unsubscribe from response topic: " + std::string(e.what()));
            }
            
            pending_requests_.erase(it);
            
            // Mark as inactive if no more pending requests
            if (pending_requests_.empty()) {
                active_.store(false);
            }
        }
        
        cJSON_Delete(response_json);
        
    } catch (const std::exception& e) {
        Utils::logError("Error processing response: " + std::string(e.what()));
    }
}

size_t TargetedRPCRequester::getPendingRequestCount() {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    return pending_requests_.size();
}

// TargetedRPCResponder implementation
TargetedRPCResponder::TargetedRPCResponder(ClientThread* client_thread, const std::string& client_id) 
    : client_(nullptr), active_(false), response_counter_(0), client_id_(client_id),
      rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    
    if (!client_thread) {
        throw DirectTemplateException("Client thread cannot be null");
    }
    
    if (client_id.empty()) {
        throw DirectTemplateException("Client ID cannot be empty");
    }
    
    // We don't own the client thread, just use it
    client_ = std::unique_ptr<ClientThread>(client_thread);
    setupRequestHandler();
    active_.store(true);
}

TargetedRPCResponder::~TargetedRPCResponder() {
    active_.store(false);
    // Release the client thread without deleting it
    client_.release();
}

void TargetedRPCResponder::setupRequestHandler() {
    if (!client_) return;
    
    // Subscribe to request topics for this specific client
    std::string request_topic = "direct_messaging/" + client_id_ + "/requests";
    try {
        client_->subscribeTopic(request_topic);
        Utils::logInfo("Subscribed to: " + request_topic);
    } catch (const std::exception& e) {
        Utils::logError("Failed to subscribe to request topic: " + std::string(e.what()));
    }
}

void TargetedRPCResponder::setRequestProcessor(const std::function<bool(const std::string&, const std::string&)>& processor) {
    request_processor_ = processor;
}

std::string TargetedRPCResponder::extractJsonValue(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t start = json.find(search);
    if (start == std::string::npos) return "unknown";
    
    start += search.length();
    size_t end = json.find("\"", start);
    if (end == std::string::npos) return "unknown";
    
    return json.substr(start, end - start);
}

cJSON* TargetedRPCResponder::createResponseJson(const std::string& transaction_id, const std::string& method, 
                                               bool success, const std::string& message, int processing_time_ms) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    cJSON* response_json = cJSON_CreateObject();
    
    cJSON_AddStringToObject(response_json, "type", "response");
    cJSON_AddStringToObject(response_json, "transaction_id", transaction_id.c_str());
    cJSON_AddStringToObject(response_json, "processed_by", client_id_.c_str());
    cJSON_AddBoolToObject(response_json, "success", success);
    cJSON_AddStringToObject(response_json, "message", message.c_str());
    cJSON_AddNumberToObject(response_json, "processing_time_ms", processing_time_ms);
    cJSON_AddNumberToObject(response_json, "timestamp", now);
    cJSON_AddStringToObject(response_json, "processed_method", method.c_str());
    
    return response_json;
}
__attribute__((weak))
bool TargetedRPCResponder::processRequest(const std::string& method, const std::string& payload) {
    if (request_processor_) {
        return request_processor_(method, payload);
    }
    
    std::uniform_int_distribution<> time_dist(200, 1000);
    int processing_time = time_dist(rng_);
    std::this_thread::sleep_for(std::chrono::milliseconds(processing_time));
    
    Utils::logInfo("Processing method '" + method + "' (simulated " + std::to_string(processing_time) + "ms)");
    
    return (method == "process_data" || method == "validate_input" || method == "generate_report");
}

__attribute__((weak))
void TargetedRPCResponder::handleRequestMessage(const std::string& topic, const std::string& payload) {
    if (!active_.load()) return;
    
    // Check if this is a request topic for this client
    std::string expected_topic = "direct_messaging/" + client_id_ + "/requests";
    if (topic != expected_topic) {
        return; // Not for this client
    }
    
    int response_num = ++response_counter_;
    Utils::logInfo("Received targeted request #" + std::to_string(response_num));
    
    try {
        // Parse request using cJSON
        cJSON* request_json = cJSON_Parse(payload.c_str());
        if (!request_json) {
            Utils::logError("Failed to parse request JSON");
            return;
        }
        
        cJSON* transaction_id_item = cJSON_GetObjectItem(request_json, "transaction_id");
        cJSON* sender_item = cJSON_GetObjectItem(request_json, "sender");
        cJSON* target_item = cJSON_GetObjectItem(request_json, "target");
        cJSON* method_item = cJSON_GetObjectItem(request_json, "method");
        cJSON* type_item = cJSON_GetObjectItem(request_json, "type");
        cJSON* response_topic_item = cJSON_GetObjectItem(request_json, "response_topic");
        
        if (!cJSON_IsString(transaction_id_item) || !cJSON_IsString(method_item) || 
            !cJSON_IsString(type_item) || !cJSON_IsString(response_topic_item)) {
            cJSON_Delete(request_json);
            Utils::logError("Invalid request JSON format");
            return;
        }
        
        std::string transaction_id = transaction_id_item->valuestring;
        std::string sender = cJSON_IsString(sender_item) ? sender_item->valuestring : "unknown";
        std::string target = cJSON_IsString(target_item) ? target_item->valuestring : "unknown";
        std::string method = method_item->valuestring;
        std::string type = type_item->valuestring;
        std::string response_topic = response_topic_item->valuestring;
        
        // Validate request
        if (type != "targeted_request") {
            sendResponse(response_topic, transaction_id, method, false, "Invalid request type", 0);
            cJSON_Delete(request_json);
            return;
        }
        
        if (target != client_id_) {
            sendResponse(response_topic, transaction_id, method, false, "Request not intended for this client", 0);
            cJSON_Delete(request_json);
            return;
        }
        
        // Process the request
        auto start_time = std::chrono::steady_clock::now();
        bool success = processRequest(method, payload);
        auto end_time = std::chrono::steady_clock::now();
        
        int processing_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();
        
        // Send response to unique topic
        std::string message = success ? "Request processed successfully" : "Request processing failed";
        sendResponse(response_topic, transaction_id, method, success, message, processing_time_ms);
        
        Utils::logInfo("Request #" + std::to_string(response_num) + " completed: " + (success ? "SUCCESS" : "FAILED"));
        
        cJSON_Delete(request_json);
        
        // Mark as inactive after processing
        active_.store(false);
        
    } catch (const std::exception& e) {
        Utils::logError("Error processing request: " + std::string(e.what()));
    }
}

void TargetedRPCResponder::sendResponse(const std::string& response_topic, const std::string& transaction_id, 
                                       const std::string& method, bool success, const std::string& message,
                                       int processing_time_ms) {
    if (!client_) return;
    
    // Create response JSON using cJSON
    cJSON* response_json = createResponseJson(transaction_id, method, success, message, processing_time_ms);
    
    char* json_string = cJSON_Print(response_json);
    if (!json_string) {
        cJSON_Delete(response_json);
        Utils::logError("Failed to create response JSON string");
        return;
    }
    
    try {
        client_->publishRawMessage(response_topic, json_string);
        Utils::logInfo("Response sent to topic: " + response_topic + " successfully");
    } catch (const std::exception& e) {
        Utils::logError("Failed to send response: " + std::string(e.what()));
    }
    
    free(json_string);
    cJSON_Delete(response_json);
}

// Generic targeted message handler
void handleTargetedMessage(const std::string& topic, const std::string& payload, 
                          TargetedRPCRequester* requester, 
                          TargetedRPCResponder* responder) {
    Utils::logInfo("Received message on topic: " + topic);
        if (requester && topic.find("direct_messaging/responses/") != std::string::npos) {
        Utils::logInfo("Message Handling as Requester Mode");
        requester->handleResponseMessage(topic, payload);
    } else {
        if (!requester) {
            Utils::logInfo("Requester condition failed: requester is null");
        }
        if (topic.find("direct_messaging/responses/") == std::string::npos) {
            Utils::logInfo("Requester condition failed: topic does not contain 'direct_messaging/responses/'");
        }
    }
    if (responder && topic.find("/requests") != std::string::npos) {
        Utils::logInfo("Message Handling as Responder Mode");
        responder->handleRequestMessage(topic, payload);
    } else {
        if (!responder) {
            Utils::logInfo("Responder condition failed: responder is null");
        }
        if (topic.find("/requests") == std::string::npos) {
            Utils::logInfo("Responder condition failed: topic does not contain '/requests'");
        }
    }
    if (!requester && !responder) {
        Utils::logInfo("No active requester or responder to handle message");
    } else if (requester && topic.find("direct_messaging/responses/") == std::string::npos && 
            responder && topic.find("/requests") == std::string::npos) {
        Utils::logInfo("Message topic doesn't match expected patterns for targeted messaging");
    } else {
        if (!requester) {
            Utils::logInfo("Targeted messaging condition: requester is null");
        } else if (topic.find("direct_messaging/responses/") != std::string::npos) {
            Utils::logInfo("Targeted messaging condition: topic contains 'direct_messaging/responses/'");
        }
        
        if (!responder) {
            Utils::logInfo("Targeted messaging condition: responder is null");
        } else if (topic.find("/requests") != std::string::npos) {
            Utils::logInfo("Targeted messaging condition: topic contains '/requests'");
        }
    }
}

// Factory functions
std::unique_ptr<TargetedRPCRequester> createTargetedRequester(ClientThread* client_thread) {
    return std::make_unique<TargetedRPCRequester>(client_thread);
}

std::unique_ptr<TargetedRPCResponder> createTargetedResponder(ClientThread* client_thread, const std::string& client_id) {
    return std::make_unique<TargetedRPCResponder>(client_thread, client_id);
}

} // namespace DirectTemplate
