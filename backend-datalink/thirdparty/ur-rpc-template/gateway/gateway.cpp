#include "gateway.h"
#include <direct_template.hpp>
#include <iostream>

using namespace DirectTemplate;

// Global client thread reference - this is what the launcher will set
DirectTemplate::ClientThread* GlobalClientThreadRef = nullptr;

// These are weak symbols that can be overridden
__attribute__((weak))
void PerformStartUpRequests(std::string& RefTopic) {
    // Can be overridden by custom implementation
    return;
}

__attribute__((weak))
void handleIncomingMessage(const std::string& topic, const std::string& payload) {
    // Default implementation - can be overridden
    std::cout << "Gateway received message on topic: " << topic << std::endl;
}

__attribute__((weak))
bool HandleRequests(const std::string& method, const std::string& payload) {
    // Can be overridden by custom implementation
    return true;
}