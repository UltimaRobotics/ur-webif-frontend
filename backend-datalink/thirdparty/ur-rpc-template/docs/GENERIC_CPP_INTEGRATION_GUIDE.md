# Generic UR-RPC C++ Client Integration Guide

## Table of Contents

1. [Overview](#overview)
2. [Architecture Components](#architecture-components)
3. [Project Setup and Dependencies](#project-setup-and-dependencies)
4. [CMake Integration](#cmake-integration)
5. [Thread Architecture Design](#thread-architecture-design)
6. [RPC Client Initialization](#rpc-client-initialization)
7. [Message Handler Implementation](#message-handler-implementation)
8. [Request/Response Pattern](#requestresponse-pattern)
9. [Configuration Management](#configuration-management)
10. [Complete Implementation Example](#complete-implementation-example)
11. [Best Practices](#best-practices)
12. [Troubleshooting](#troubleshooting)

---

## Overview

This guide demonstrates how to integrate the **UR-RPC C++ wrapper** (`thirdparty/ur-rpc-template/pkg_src/api/wrappers/cpp`) into any C++ application with proper thread management using the **ur-threadder-api**. The UR-RPC framework provides MQTT-based RPC communication with modern C++ interfaces, RAII resource management, and exception safety.

### Key Benefits

- **Thread-safe RPC communication** over MQTT
- **Modern C++ API** with smart pointers and RAII
- **Asynchronous message handling** with callbacks
- **Independent thread execution** using ur-threadder-api
- **Flexible topic subscription** and message routing
- **JSON-based configuration** for easy deployment

---

## Architecture Components

### 1. UR-RPC C++ Wrapper
**Location**: `thirdparty/ur-rpc-template/pkg_src/api/wrappers/cpp/`

The C++ wrapper provides:
- `UrRpc::Library` - RAII library initialization
- `UrRpc::Client` - Main RPC client interface
- `UrRpc::ClientConfig` - Configuration builder
- `UrRpc::TopicConfig` - Topic structure management
- `UrRpc::JsonValue` - Smart JSON wrapper

### 2. UR-Threadder API
**Location**: `ur-threadder-api/`

Thread management library providing:
- `ThreadManager` - Thread lifecycle management
- Thread registration and attachment
- Process execution support
- Thread synchronization primitives

### 3. Integration Components

Your application needs:
- **RPC Client Wrapper Class** - Encapsulates UR-RPC client
- **Thread Management** - Handles RPC client thread lifecycle
- **Message Handlers** - Process incoming RPC messages
- **Configuration Loader** - Reads and applies RPC configuration

---

## Project Setup and Dependencies

### Directory Structure

```
your-binary/
├── CMakeLists.txt
├── include/
│   └── RpcClient.hpp          # Your RPC client wrapper
├── src/
│   ├── RpcClient.cpp          # Implementation
│   └── main.cpp               # Application entry point
├── config/
│   └── ur-rpc-config.json     # RPC configuration
└── thirdparty/
    └── ur-rpc-template/       # UR-RPC dependency (submodule/copy)
```

### Required Dependencies

1. **UR-RPC Template Library** (C wrapper + C++ wrapper)
2. **MQTT Client Library** (mosquitto)
3. **cJSON Library** (JSON parsing)
4. **UR-Logger API** (optional, for logging)
5. **UR-Threadder API** (thread management)

---

## CMake Integration

### Step 1: Add UR-RPC as Subdirectory

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(YourBinary)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add UR-RPC template library
add_subdirectory(thirdparty/ur-rpc-template)

# Add UR-Threadder API
add_subdirectory(thirdparty/ur-threadder-api)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/nholmann  # For JSON
    ${THREADDER_INCLUDE_DIRS}
    ${RPC_TEMPLATE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/build  # For generated headers
)

# Note: RPC_TEMPLATE_INCLUDE_DIRS should include ALL of these paths:
# Base paths:
# - thirdparty/ur-rpc-template
# - thirdparty/ur-rpc-template/extensions
# 
# Dependency paths:
# - thirdparty/ur-rpc-template/deps/cJSON
# - thirdparty/ur-rpc-template/deps/mqtt-client/src
# - thirdparty/ur-rpc-template/deps/nlohmann/include
# - thirdparty/ur-rpc-template/deps/ur-logger-api
# 
# C++ wrapper paths (CRITICAL - often missed):
# - thirdparty/ur-rpc-template/pkg_src/api/wrappers/cpp
# - thirdparty/ur-rpc-template/pkg_src/api/wrappers/cpp/extensions
# - thirdparty/ur-rpc-template/pkg_src/api/wrappers/cpp/gateway
#
# Build directory (for generated files):
# - ${CMAKE_CURRENT_SOURCE_DIR}/build
```

### Step 2: Create Your Executable

```cmake
# Source files
set(SOURCES
    src/main.cpp
    src/RpcClient.cpp
)

# Create executable
add_executable(your-binary ${SOURCES})

# Link libraries
target_link_libraries(your-binary
    ${THREADDER_LIBS}      # UR-Threadder API (includes threadmanager and threadmanager_cpp)
    ${RPC_TEMPLATE_LIBS}   # UR-RPC template library (includes ur-rpc-template, mqtt-client, cJSON, ur-logger-api)
    Threads::Threads       # POSIX threads (modern CMake approach)
)

# Note: The RPC_TEMPLATE_LIBS variable internally includes all required dependencies:
# - ur-rpc-template (main library)
# - mqtt-client (MQTT protocol implementation)
# - cJSON (JSON parsing)
# - ur-logger-api (logging functionality)
#
# Using variables (${RPC_TEMPLATE_LIBS}, ${THREADDER_LIBS}) instead of direct library
# names ensures compatibility across different build configurations and platforms.
```

### Step 3: Compiler Flags

```cmake
# Enable warnings and optimizations
target_compile_options(your-binary PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Debug>:-g -O0>
)
```

---

## Thread Architecture Design

### Thread Lifecycle Overview

```
┌─────────────────────────────────────────────────────────┐
│                   Main Thread                           │
│  1. Initialize ThreadManager                            │
│  2. Create RpcClient instance                           │
│  3. Launch RPC thread via ThreadManager                 │
│  4. Continue application logic                     │
└─────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                   RPC Client Thread                     │
│  1. Initialize UR-RPC library                           │
│  2. Load configuration from JSON                        │
│  3. Create and configure client                         │
│  4. Connect to MQTT broker                              │
│  5. Start message loop (blocking)                       │
│  6. Process messages via callbacks                      │
└─────────────────────────────────────────────────────────┘
```

### RpcClient Header Design

**File**: `include/RpcClient.hpp`

```cpp
#pragma once

#include <memory>
#include <string>
#include <atomic>
#include <functional>
#include <mutex>

// Forward declarations for UR-RPC types
namespace UrRpc {
    class Library;
    class Client;
    enum class ConnectionStatus;
}

class RpcClient {
public:
    /**
     * @brief Constructor
     * @param configPath Path to ur-rpc-config.json
     * @param clientId Client identifier for RPC
     */
    RpcClient(const std::string& configPath, const std::string& clientId);

    /**
     * @brief Destructor - automatically cleans up resources
     */
    ~RpcClient();

    /**
     * @brief Start the RPC client (blocking call for thread)
     * @return true if started successfully
     */
    bool start();

    /**
     * @brief Stop the RPC client
     */
    void stop();

    /**
     * @brief Check if RPC client is running
     * @return true if running
     */
    bool isRunning() const;

    /**
     * @brief Send a notification (fire-and-forget)
     * @param target Target client ID
     * @param method Method name
     * @param params Parameters as JSON string
     */
    void sendNotification(const std::string& target, 
                         const std::string& method,
                         const std::string& params);

    /**
     * @brief Send a request and wait for response
     * @param target Target client ID
     * @param method Method name
     * @param params Parameters as JSON string
     * @param callback Response callback (success, response)
     * @param timeout Timeout in milliseconds
     */
    void sendRequest(const std::string& target,
                    const std::string& method,
                    const std::string& params,
                    std::function<void(bool, const std::string&)> callback,
                    int timeout = 5000);

    /**
     * @brief Set custom message handler for specific topic patterns
     * @param handler Callback function (topic, payload)
     */
    void setMessageHandler(
        std::function<void(const std::string&, const std::string&)> handler
    );

private:
    std::string configPath_;
    std::string clientId_;
    std::shared_ptr<UrRpc::Library> library_;
    std::shared_ptr<UrRpc::Client> client_;
    std::atomic<bool> running_;
    std::function<void(const std::string&, const std::string&)> messageHandler_;

    void handleConnectionStatus(UrRpc::ConnectionStatus status);
    void handleMessage(const char* topic, const char* payload, int payload_len);
};
```

---

## RPC Client Initialization

### Implementation File

**File**: `src/RpcClient.cpp`

```cpp
#include "RpcClient.hpp"
#include "ur-rpc-template.hpp"
#include <iostream>
#include <fstream>

// Example using ur-mavdiscovery as reference
RpcClient::RpcClient(const std::string& configPath, const std::string& clientId)
    : configPath_(configPath)
    , clientId_(clientId)
    , library_(nullptr)
    , client_(nullptr)
    , running_(false)
{
    std::cout << "[RPC] Client initialized with config: " << configPath_ << std::endl;
}

RpcClient::~RpcClient() {
    if (running_.load()) {
        stop();
    }
}

bool RpcClient::start() {
    if (running_.load()) {
        std::cerr << "[RPC] Client already running" << std::endl;
        return true;
    }

    try {
        // Step 1: Initialize the UR-RPC library (RAII)
        library_ = std::make_shared<UrRpc::Library>();

        std::cout << "[RPC] Creating client with config: " << configPath_ << std::endl;

        // Step 2: Create client configuration from file
        UrRpc::ClientConfig config;
        config.loadFromFile(configPath_);

        // Override client ID if specified
        if (!clientId_.empty()) {
            config.setClientId(clientId_);
        }

        // Step 3: Create topic configuration
        UrRpc::TopicConfig topicConfig;
        // Format: direct_messaging/{client_id}/{suffix}
        topicConfig.setPrefixes("direct_messaging", clientId_);
        topicConfig.setSuffixes("requests", "responses", "notifications");

        // Step 4: Create the RPC client
        client_ = std::make_shared<UrRpc::Client>(config, topicConfig);

        if (!client_) {
            std::cerr << "[RPC] Failed to create client instance" << std::endl;
            library_.reset();
            return false;
        }

        // Step 5: Set connection callback
        client_->setConnectionCallback(
            [this](UrRpc::ConnectionStatus status) {
                this->handleConnectionStatus(status);
            }
        );

        // Step 6: Set message handler
        client_->setMessageHandler(
            [this](const std::string& topic, const std::string& payload) {
                this->handleMessage(topic.c_str(), payload.c_str(), payload.length());
            }
        );

        std::cout << "[RPC] Connecting to MQTT broker..." << std::endl;

        // Step 7: Connect to MQTT broker
        client_->connect();

        std::cout << "[RPC] Starting client loop..." << std::endl;

        // Step 8: Start the client (blocking call - runs in thread)
        client_->start();

        running_.store(true);
        std::cout << "[RPC] Client started successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[RPC] Exception during start: " << e.what() << std::endl;
        client_.reset();
        library_.reset();
        return false;
    }
}

void RpcClient::stop() {
    if (!running_.load()) {
        return;
    }

    std::cout << "[RPC] Stopping client..." << std::endl;

    if (client_) {
        client_->stop();
        client_->disconnect();
        client_.reset();
    }

    if (library_) {
        library_.reset();
    }

    running_.store(false);
    std::cout << "[RPC] Client stopped" << std::endl;
}

bool RpcClient::isRunning() const {
    return running_.load();
}

void RpcClient::handleConnectionStatus(UrRpc::ConnectionStatus status) {
    switch (status) {
        case UrRpc::ConnectionStatus::Connected:
            std::cout << "[RPC] Connected to broker" << std::endl;
            break;
        case UrRpc::ConnectionStatus::Disconnected:
            std::cerr << "[RPC] Disconnected from broker" << std::endl;
            break;
        case UrRpc::ConnectionStatus::Connecting:
            std::cout << "[RPC] Connecting to broker..." << std::endl;
            break;
        case UrRpc::ConnectionStatus::Error:
            std::cerr << "[RPC] Connection error" << std::endl;
            break;
    }
}

void RpcClient::handleMessage(const char* topic, const char* payload, int payload_len) {
    std::string topicStr(topic);
    std::string payloadStr(payload, payload_len);

    std::cout << "[RPC] Message received on topic: " << topicStr << std::endl;

    // Call custom handler if set
    if (messageHandler_) {
        messageHandler_(topicStr, payloadStr);
    }
}

void RpcClient::setMessageHandler(
    std::function<void(const std::string&, const std::string&)> handler
) {
    messageHandler_ = handler;
}
```

---

## Message Handler Implementation

### Handling Incoming RPC Requests

```cpp
void RpcClient::handleMessage(const char* topic, const char* payload, int payload_len) {
    std::string topicStr(topic);
    std::string payloadStr(payload, payload_len);

    // Example: Handle requests on "requests" topic
    if (topicStr.find("/requests") != std::string::npos) {
        try {
            // Parse JSON-RPC request
            nlohmann::json requestJson = nlohmann::json::parse(payloadStr);

            if (!requestJson.contains("method") || !requestJson.contains("id")) {
                std::cerr << "[RPC] Malformed request - missing method or id" << std::endl;
                return;
            }

            std::string method = requestJson["method"];
            std::string requestId = requestJson["id"];

            std::cout << "[RPC] Request: method=" << method << ", id=" << requestId << std::endl;

            // Route to specific handlers
            if (method == "get_status") {
                handleGetStatus(requestId, requestJson["params"]);
            } else if (method == "set_config") {
                handleSetConfig(requestId, requestJson["params"]);
            } else {
                sendErrorResponse(requestId, -32601, "Method not found: " + method);
            }

        } catch (const std::exception& e) {
            std::cerr << "[RPC] Error parsing request: " << e.what() << std::endl;
        }
    }

    // Custom handler for other topics
    if (messageHandler_) {
        messageHandler_(topicStr, payloadStr);
    }
}

void RpcClient::sendErrorResponse(const std::string& requestId, 
                                   int errorCode, 
                                   const std::string& errorMessage) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["id"] = requestId;
    response["error"] = {
        {"code", errorCode},
        {"message", errorMessage}
    };

    std::string responseTopic = "direct_messaging/" + clientId_ + "/responses";
    client_->publishMessage(responseTopic, response.dump());
}
```

---

## Request/Response Pattern

### Requester Implementation

**Sending a Request and Handling Response**

```cpp
void RpcClient::sendRequest(const std::string& target,
                           const std::string& method,
                           const std::string& params,
                           std::function<void(bool, const std::string&)> callback,
                           int timeout) {
    if (!running_.load() || !client_) {
        std::cerr << "[RPC] Client not running" << std::endl;
        if (callback) {
            callback(false, "Client not running");
        }
        return;
    }

    try {
        // Create JSON params
        UrRpc::JsonValue jsonParams(params);

        // Create a Request object
        UrRpc::Request request;
        request.setMethod(method, target);
        request.setAuthority(UrRpc::Authority::User);
        request.setParams(jsonParams);
        request.setTimeout(timeout);

        // Use callAsync for non-blocking request
        client_->callAsync(
            request,
            [callback](bool success, 
                      const UrRpc::JsonValue& result, 
                      const std::string& error_message, 
                      int error_code) {
                if (callback) {
                    if (success) {
                        callback(true, result.toString());
                    } else {
                        callback(false, error_message);
                    }
                }
            }
        );

        std::cout << "[RPC] Sent request to " << target 
                  << ": method=" << method << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[RPC] Failed to send request: " << e.what() << std::endl;
        if (callback) {
            callback(false, e.what());
        }
    }
}
```

### Responder Implementation

**Handling Requests and Sending Responses**

```cpp
void RpcClient::handleGetStatus(const std::string& requestId, 
                                const nlohmann::json& params) {
    std::cout << "[RPC] Handling get_status request" << std::endl;

    // Process the request
    nlohmann::json result;
    result["status"] = "operational";
    result["uptime"] = getUptime();
    result["version"] = "1.0.0";

    // Send success response
    sendSuccessResponse(requestId, result);
}

void RpcClient::sendSuccessResponse(const std::string& requestId, 
                                    const nlohmann::json& result) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["id"] = requestId;
    response["result"] = result;

    std::string responseTopic = "direct_messaging/" + clientId_ + "/responses";
    client_->publishMessage(responseTopic, response.dump());

    std::cout << "[RPC] Sent success response for request: " << requestId << std::endl;
}
```

---

## Configuration Management

### JSON Configuration File

**File**: `config/ur-rpc-config.json`

```json
{
  "client_id": "your-service-name",
  "broker_host": "127.0.0.1",
  "broker_port": 1899,
  "keepalive": 60,
  "qos": 1,
  "auto_reconnect": true,
  "reconnect_delay_min": 1,
  "reconnect_delay_max": 60,
  "use_tls": false,
  "heartbeat": {
    "enabled": true,
    "interval_seconds": 5,
    "topic": "clients/your-service-name/heartbeat",
    "payload": "{\"client\":\"your-service-name\",\"status\":\"alive\"}"
  },
  "json_added_pubs": {
    "topics": [
      "direct_messaging/your-service-name/notifications"
    ]
  },
  "json_added_subs": {
    "topics": [
      "direct_messaging/your-service-name/requests"
    ]
  }
}
```

### Loading Configuration

```cpp
// In RpcClient::start()
UrRpc::ClientConfig config;
config.loadFromFile(configPath_);

// Optional: Override specific settings programmatically
config.setBrokerHost("127.0.0.1")
      .setBrokerPort(1899)
      .setClientId("your-service-name")
      .setKeepAlive(60);
```

---

## Complete Implementation Example

### Main Application Entry Point

**File**: `src/main.cpp`

```cpp
#include "RpcClient.hpp"
#include "ThreadManager.hpp"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>

// Global flag for graceful shutdown
static std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    std::cout << "\n[Main] Caught signal " << signal << ", shutting down..." << std::endl;
    g_running.store(false);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string configPath = "config/ur-rpc-config.json";
    std::string clientId = "your-service-name";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-c" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "-i" && i + 1 < argc) {
            clientId = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [-c config.json] [-i client_id]" << std::endl;
            return 0;
        }
    }

    std::cout << "[Main] Starting application..." << std::endl;
    std::cout << "[Main] Config: " << configPath << std::endl;
    std::cout << "[Main] Client ID: " << clientId << std::endl;

    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Step 1: Initialize ThreadManager
    ThreadMgr::ThreadManager threadManager;

    // Step 2: Create RpcClient instance
    auto rpcClient = std::make_shared<RpcClient>(configPath, clientId);

    // Step 3: Set custom message handler (optional)
    rpcClient->setMessageHandler(
        [](const std::string& topic, const std::string& payload) {
            std::cout << "[Handler] Custom message: " << topic << std::endl;
            // Process custom messages here
        }
    );

    // Step 4: Create and launch RPC thread
    unsigned int rpcThreadId = 0;

    try {
        rpcThreadId = threadManager.createThread(
            [rpcClient]() {
                std::cout << "[RPC Thread] Starting RPC client..." << std::endl;

                // This is a blocking call that runs the RPC event loop
                if (!rpcClient->start()) {
                    std::cerr << "[RPC Thread] Failed to start RPC client" << std::endl;
                    return;
                }

                std::cout << "[RPC Thread] RPC client stopped" << std::endl;
            }
        );

        std::cout << "[Main] RPC thread created with ID: " << rpcThreadId << std::endl;

        // Step 5: Register thread with attachment identifier
        threadManager.registerThread(rpcThreadId, "rpc-client-thread");

    } catch (const std::exception& e) {
        std::cerr << "[Main] Failed to create RPC thread: " << e.what() << std::endl;
        return 1;
    }

    // Step 6: Wait for RPC client to be ready
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (!rpcClient->isRunning()) {
        std::cerr << "[Main] RPC client failed to start" << std::endl;
        return 1;
    }

    std::cout << "[Main] RPC client is running" << std::endl;

    // Step 7: Main application loop
    while (g_running.load()) {
        // Your main application logic here

        // Example: Send periodic status
        if (rpcClient->isRunning()) {
            // Could send notifications or perform other RPC operations
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Step 8: Graceful shutdown
    std::cout << "[Main] Shutting down..." << std::endl;

    // Stop RPC client
    rpcClient->stop();

    // Wait for RPC thread to finish
    threadManager.joinThread(rpcThreadId, std::chrono::seconds(5));

    std::cout << "[Main] Application stopped" << std::endl;
    return 0;
}
```

---

## Best Practices

### 1. Thread Safety

```cpp
// Always use atomic flags for thread communication
std::atomic<bool> running_{false};

// Use mutexes for shared data access
std::mutex dataMutex_;
std::lock_guard<std::mutex> lock(dataMutex_);
```

### 2. Resource Management

```cpp
// Use RAII and smart pointers
std::shared_ptr<UrRpc::Library> library_;
std::shared_ptr<UrRpc::Client> client_;

// Automatic cleanup in destructor
~RpcClient() {
    if (running_.load()) {
        stop();
    }
}
```

### 3. Error Handling

```cpp
try {
    client_->connect();
    client_->start();
} catch (const UrRpc::Exception& e) {
    std::cerr << "[RPC] Error: " << e.what() << std::endl;
    return false;
} catch (const std::exception& e) {
    std::cerr << "[RPC] Unexpected error: " << e.what() << std::endl;
    return false;
}
```

### 4. Logging

```cpp
// Use consistent log prefixes
std::cout << "[RPC] " << message << std::endl;
std::cerr << "[RPC] ERROR: " << error << std::endl;

// Consider integrating with a logging framework
```

### 5. Configuration Validation

```cpp
bool validateConfig(const std::string& configPath) {
    std::ifstream file(configPath);
    if (!file.good()) {
        std::cerr << "[RPC] Config file not found: " << configPath << std::endl;
        return false;
    }

    try {
        nlohmann::json config = nlohmann::json::parse(file);
        // Validate required fields
        if (!config.contains("broker_host") || !config.contains("broker_port")) {
            std::cerr << "[RPC] Invalid config: missing broker settings" << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[RPC] Invalid JSON: " << e.what() << std::endl;
        return false;
    }
}
```

---

## Troubleshooting

### Common Issues

#### 1. Client Won't Connect

**Problem**: RPC client fails to connect to MQTT broker

**Solutions**:
- Verify broker is running: `netstat -tulpn | grep 1899`
- Check configuration: broker_host and broker_port
- Verify network connectivity
- Check firewall settings

```cpp
// Add connection timeout
config.setTimeout(10000); // 10 seconds
```

#### 2. Messages Not Received

**Problem**: Message handler not being called

**Solutions**:
- Verify topic subscriptions in config
- Check topic pattern matching
- Ensure message handler is set before client starts

```cpp
// Set handler before starting client
client_->setMessageHandler([](const std::string& topic, const std::string& payload) {
    std::cout << "Message: " << topic << std::endl;
});
client_->start();
```

#### 3. Thread Not Starting

**Problem**: RPC thread fails to start

**Solutions**:
- Check ThreadManager initialization
- Verify thread function signature
- Check for exceptions in thread function

```cpp
try {
    threadId = threadManager.createThread([&]() {
        try {
            rpcClient->start();
        } catch (const std::exception& e) {
            std::cerr << "Thread error: " << e.what() << std::endl;
        }
    });
} catch (const std::exception& e) {
    std::cerr << "Failed to create thread: " << e.what() << std::endl;
}
```

#### 4. Memory Leaks

**Problem**: Memory leaks detected

**Solutions**:
- Use smart pointers consistently
- Ensure proper cleanup in destructors
- Use valgrind for leak detection

```bash
valgrind --leak-check=full ./your-binary
```

#### 5. Deadlocks

**Problem**: Application hangs or deadlocks

**Solutions**:
- Avoid holding locks when calling RPC methods
- Use timeout for blocking operations
- Don't call stop() from within callbacks

```cpp
// Bad: Holding lock when making RPC call
{
    std::lock_guard<std::mutex> lock(mutex_);
    client_->sendRequest(...); // May deadlock
}

// Good: Release lock before RPC call
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Prepare data
}
client_->sendRequest(...); // Call after releasing lock
```

---

## Advanced Topics

### 1. Multiple RPC Clients

```cpp
// Create multiple clients with different configurations
auto client1 = std::make_shared<RpcClient>("config/rpc1.json", "service-1");
auto client2 = std::make_shared<RpcClient>("config/rpc2.json", "service-2");

// Launch in separate threads
unsigned int thread1 = threadManager.createThread([client1]() { client1->start(); });
unsigned int thread2 = threadManager.createThread([client2]() { client2->start(); });

// Register with unique identifiers
threadManager.registerThread(thread1, "rpc-client-1");
threadManager.registerThread(thread2, "rpc-client-2");
```

### 2. Request Queuing

```cpp
class RpcClient {
private:
    std::queue<PendingRequest> requestQueue_;
    std::mutex queueMutex_;

    void processRequestQueue() {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!requestQueue_.empty()) {
            auto request = requestQueue_.front();
            requestQueue_.pop();
            sendRequestInternal(request);
        }
    }
};
```

### 3. SSL/TLS Support

```json
{
  "use_tls": true,
  "tls": {
    "ca_cert": "/path/to/ca.crt",
    "client_cert": "/path/to/client.crt",
    "client_key": "/path/to/client.key",
    "verify_peer": true
  }
}
```

### 4. Heartbeat Monitoring

```cpp
void RpcClient::startHeartbeatMonitor() {
    heartbeatThread_ = std::thread([this]() {
        while (running_.load()) {
            if (!isConnected()) {
                std::cerr << "[RPC] Lost connection, attempting reconnect..." << std::endl;
                reconnect();
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    });
}
```

---

## Conclusion

This guide provides a comprehensive overview of integrating the UR-RPC C++ client into any application with proper thread management. Key takeaways:

1. **Use RAII** for automatic resource management
2. **Leverage ThreadManager** for robust thread lifecycle control
3. **Implement proper error handling** with try-catch blocks
4. **Design thread-safe** message handlers
5. **Follow request/response patterns** for bidirectional communication
6. **Validate configurations** before starting
7. **Monitor connections** and implement reconnection logic

For more examples, refer to the ur-mavdiscovery implementation in `ur-mavlink-stack/ur-mavdiscovery-v1.1/src/RpcClient.cpp`.

---

## Additional Resources

- **UR-RPC Template Documentation**: `thirdparty/ur-rpc-template/UR_RPC_TEMPLATE_DOCUMENTATION.md`
- **C++ Wrapper Coverage**: `thirdparty/ur-rpc-template/pkg_src/api/wrappers/cpp/C_API_FEATURE_COVERAGE.md`
- **Thread Manager API**: `ur-threadder-api/README.md`
- **Example Clients**: `thirdparty/ur-rpc-template/clients/wrapper_cpp/`

---

**Version**: 1.0  
**Last Updated**: 2025-01-28  
**Maintainer**: UR-RPC Development Team