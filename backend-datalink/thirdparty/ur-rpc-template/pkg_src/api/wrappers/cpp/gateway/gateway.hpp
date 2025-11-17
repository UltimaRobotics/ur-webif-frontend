#ifndef GATEWAY_HPP
#define GATEWAY_HPP

#include <direct_template.hpp>
#include <atomic>

extern DirectTemplate::GlobalClient& GlobalClientRef;
extern std::unique_ptr<DirectTemplate::TargetedRPCRequester> g_requester ;
extern std::unique_ptr<DirectTemplate::TargetedRPCResponder> g_responder ;

// Global variables for client management
extern std::atomic<bool> g_running;
extern DirectTemplate::ClientThread*  GlobalClientThraedRef;

void RpcClientThread(std::string configPath);
void PerformStartUpRequests(std::string& RefTopic) __attribute__((weak));
void handleIncomingMessage(const std::string& topic, const std::string& payload)__attribute__((weak));
bool HandleRequests(const std::string& method, const std::string& payload) __attribute__((weak));
#endif
