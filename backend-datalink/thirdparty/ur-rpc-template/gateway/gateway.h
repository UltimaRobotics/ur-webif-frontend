

#ifndef GATEWAY_H
#define GATEWAY_H

#include <atomic>
#include <string>

// Forward declarations
namespace DirectTemplate {
    class ClientThread;
    class TargetedRPCRequester;
    class TargetedRPCResponder;
}

// Global client reference (matches the implementation in gateway.cpp)
// Note: The variable name has a typo in the original code (Thraed instead of Thread)
extern DirectTemplate::ClientThread* GlobalClientThraedRef;

// These weak symbols are defined in gateway.cpp but can be overridden
void PerformStartUpRequests(std::string& RefTopic) __attribute__((weak));
void handleIncomingMessage(const std::string& topic, const std::string& payload) __attribute__((weak));
bool HandleRequests(const std::string& method, const std::string& payload) __attribute__((weak));

#endif // GATEWAY_H
