#ifndef MANAGED_WEBSOCKET_SERVER_H
#define MANAGED_WEBSOCKET_SERVER_H

#include "websocket_server.h"
#include "ThreadManager.hpp"
#include <memory>
#include <string>
#include <functional>

class ManagedWebSocketServer {
public:
    typedef std::function<void(const std::string&, const nlohmann::json&)> MessageHandler;
    typedef std::function<void(const std::string&)> ConnectionHandler;

    ManagedWebSocketServer();
    ~ManagedWebSocketServer();

    bool start(const ConfigLoader::WebSocketConfig& config);
    void stop();
    bool isRunning() const { return is_running_; }

    void setMessageHandler(MessageHandler handler) { message_handler_ = handler; }
    void setConnectionOpenHandler(ConnectionHandler handler) { connection_open_handler_ = handler; }
    void setConnectionCloseHandler(ConnectionHandler handler) { connection_close_handler_ = handler; }

    void broadcast(const nlohmann::json& message);
    void sendToClient(const std::string& connection_id, const nlohmann::json& message);
    size_t getConnectionCount() const;

    // Thread management via ur-threadder-api
    bool pause();
    bool resume();
    bool restart();
    ThreadMgr::ThreadState getState() const;
    unsigned int getThreadId() const { return thread_id_; }

private:
    std::unique_ptr<WebSocketServer> websocket_server_;
    std::unique_ptr<ThreadMgr::ThreadManager> thread_manager_;
    ConfigLoader::WebSocketConfig config_;
    unsigned int thread_id_;
    bool is_running_;

    MessageHandler message_handler_;
    ConnectionHandler connection_open_handler_;
    ConnectionHandler connection_close_handler_;

    // Thread function wrapper
    void websocketServerThread();
    void log(const std::string& message) const;
};

#endif // MANAGED_WEBSOCKET_SERVER_H
