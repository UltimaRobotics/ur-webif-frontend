
#include <gateway.hpp>
#include <direct_template.hpp>

DirectTemplate::GlobalClient& GlobalClientRef = DirectTemplate::GlobalClient::getInstance();
std::unique_ptr<DirectTemplate::TargetedRPCRequester> g_requester = nullptr;
std::unique_ptr<DirectTemplate::TargetedRPCResponder> g_responder = nullptr;

std::atomic<bool> g_running(true);
DirectTemplate::ClientThread* GlobalClientThraedRef = nullptr;

using namespace DirectTemplate;

__attribute__((weak))
void PerformStartUpRequests(std::string& RefTopic) {
    //Overridden by the Custom implemented startup for each package
    return;
}
__attribute__((weak))
void handleIncomingMessage(const std::string& topic, const std::string& payload) {
    //Overridden by the Custom implemented handling for each package
    handleTargetedMessage(topic, payload, g_requester.get(), g_responder.get());
}
__attribute__((weak))
bool HandleRequests(const std::string& method, const std::string& payload) {
    //Overridden by the Custom implemented handling for each package
    return true;
};

void RpcClientThread(std::string configPath) {
    try {
        g_running.store(true);

        GlobalClient& globalClient = GlobalClient::getInstance();
        if (!globalClient.initialize(configPath)) {
            throw DirectTemplateException("Failed to initialize global client");
        }
        Utils::logInfo("Global client initialized successfully");

        ClientThread clientThread(configPath);
        GlobalClientThraedRef = &clientThread;
        
        std::string client_id = globalClient.client_->config.client_id;
        if (client_id.empty()) {
            client_id = "default_responder"; 
        }
        Utils::logInfo("Using client ID: " + client_id);
        
        ReconnectConfig reconnectConfig(10, 500, true);
        clientThread.setReconnectConfig(reconnectConfig);
        clientThread.setMessageHandler([](const std::string& topic, const std::string& payload) {
            ::handleIncomingMessage(topic, payload);
        });        
        clientThread.setConnectionStatusCallback([client_id](bool connected, const std::string& reason) {
            if (connected) {
                Utils::logInfo("[" + client_id + "] Connected: " + reason);
            } else {
                Utils::logError("[" + client_id + "] Disconnected: " + reason);
            }
        });

        if (!clientThread.start()) {
            throw DirectTemplateException("Failed to start client thread");
        }
        Utils::logInfo("[" + client_id + "] Client thread started");        

        int connection_attempts = 0;
        while (!clientThread.isConnected() && connection_attempts < 20 && g_running.load()) {
            Utils::logInfo("[" + client_id + "] Waiting for thread connection... (" + std::to_string(connection_attempts + 1) + "/20)");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            connection_attempts++;
        }

        if (clientThread.isConnected()) {
            Utils::logInfo("[" + client_id + "] Client thread connected successfully");

            // Subscribe to the direct messaging requests topic
            std::string request_topic = "direct_messaging/" + client_id + "/requests";
            clientThread.subscribeTopic(request_topic);
            Utils::logInfo("[" + client_id + "] Subscribed to topic: " + request_topic);

            // Create targeted RPC requester and responder
            g_requester = std::make_unique<TargetedRPCRequester>(&clientThread);
            g_responder = std::make_unique<TargetedRPCResponder>(&clientThread, client_id);
            g_responder->setRequestProcessor(HandleRequests);
            
            Utils::logInfo("[" + client_id + "] Targeted RPC Requester and Responder created successfully");
        } else {
            Utils::logError("[" + client_id + "] Client thread failed to connect");
            g_running.store(false);
            return;
        }

        while (clientThread.isConnected() && g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        Utils::logInfo("[" + client_id + "] Stopping client thread...");
        clientThread.stopHeartbeat();
        if (!clientThread.stop()) {
            Utils::logError("[" + client_id + "] Failed to stop client thread gracefully");
        }

        globalClient.cleanup();
        Utils::logInfo("[" + client_id + "] Global client cleaned up");

    } catch (const DirectTemplateException& e) {
        Utils::logError("Client Thread Error: " + std::string(e.what()));
        g_running.store(false);
    } catch (const std::exception& e) {
        Utils::logError("Unexpected thread error: " + std::string(e.what()));
        g_running.store(false);
    }
}
