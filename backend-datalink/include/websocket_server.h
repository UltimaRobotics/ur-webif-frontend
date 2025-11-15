#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <thread>
#include <atomic>
#include <memory>
#include "config_loader.h"

using json = nlohmann::json;

typedef websocketpp::server<websocketpp::config::asio> server;
typedef server::message_ptr message_ptr;
typedef websocketpp::connection_hdl connection_hdl;

// Custom hash functor for connection_hdl to fix GCC 11 compatibility
struct connection_hdl_hash {
    size_t operator()(const connection_hdl& hdl) const {
        // Use the raw pointer value - expired connections will be nullptr which is fine
        return std::hash<void*>()(hdl.lock().get());
    }
};

// Custom equal_to functor for connection_hdl to fix GCC 11 compatibility
struct connection_hdl_equal {
    bool operator()(const connection_hdl& a, const connection_hdl& b) const {
        // Use owner_before which works properly with weak_ptr<void>
        return !a.owner_before(b) && !b.owner_before(a);
    }
};

class WebSocketServer {
public:
    typedef std::function<void(const std::string&, const json&)> MessageHandler;
    typedef std::function<void(const std::string&)> ConnectionHandler;

    WebSocketServer();
    ~WebSocketServer();

    bool start(const ConfigLoader::WebSocketConfig& config);
    void stop();
    bool isRunning() const { return running_.load(); }

    void setMessageHandler(MessageHandler handler) { message_handler_ = handler; }
    void setConnectionOpenHandler(ConnectionHandler handler) { connection_open_handler_ = handler; }
    void setConnectionCloseHandler(ConnectionHandler handler) { connection_close_handler_ = handler; }

    void broadcast(const json& message);
    void sendToClient(const std::string& connection_id, const json& message);
    size_t getConnectionCount() const { return connections_.size(); }

private:
    server server_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    ConfigLoader::WebSocketConfig config_;

    std::unordered_map<connection_hdl, std::string, 
                       connection_hdl_hash, 
                       connection_hdl_equal> connections_;
    std::mutex connections_mutex_;

    MessageHandler message_handler_;
    ConnectionHandler connection_open_handler_;
    ConnectionHandler connection_close_handler_;

    void onOpen(connection_hdl hdl);
    void onClose(connection_hdl hdl);
    void onMessage(connection_hdl hdl, message_ptr msg);
    void onError(connection_hdl hdl);

    std::string generateConnectionId() const;
    void log(const std::string& message) const;
};

#endif // WEBSOCKET_SERVER_H
