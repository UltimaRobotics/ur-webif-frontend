#pragma once

#include <string>
#include <chrono>

namespace FrontendPP {
    namespace BuildAttributes {
        // Build-time configuration
        constexpr const char* BUILD_TIMESTAMP = "2025-11-15 13:21:05 UTC";
        constexpr const char* GIT_COMMIT_HASH = "222937a";
        constexpr const char* BUILD_TYPE = "";
        constexpr const char* COMPILER_VERSION = "GCC 11.4.0";
        
        // Default hardcoded credentials (for initial database creation)
        constexpr const char* DEFAULT_ADMIN_USERNAME = "admin";
        constexpr const char* DEFAULT_ADMIN_PASSWORD = "admin123";
        constexpr const char* DEFAULT_ADMIN_EMAIL = "admin@frontendpp.local";
        
        // Default encryption key (hex-encoded AES-256 key)
        constexpr const char* DEFAULT_ENCRYPTION_KEY = "2b7e151628aed2a6abf7158809cf4f3c762e7160f38b4da56a784d9045190abc";
        
        // Default JWT secret (will be overridden at runtime)
        constexpr const char* DEFAULT_JWT_SECRET = "your-256-bit-secret-key-change-this-in-production";
        
        // Application metadata
        constexpr const char* APP_NAME = "FrontendPP";
        constexpr const char* APP_VERSION = "2.0.0";
        constexpr const char* APP_DESCRIPTION = "Production C++ Web Server with Enhanced Authentication";
        
        // Build flags
        constexpr bool DEBUG_BUILD = false;
        constexpr bool VERBOSE_ENABLED = true;
        constexpr bool ENCRYPTION_ENABLED = true;
        
        // Helper functions
        inline std::string get_build_info() {
            return std::string(APP_NAME) + " v" + APP_VERSION + 
                   " (" + BUILD_TYPE + ") built on " + BUILD_TIMESTAMP;
        }
        
        inline std::string get_credential_info() {
            return "Default credentials: " + std::string(DEFAULT_ADMIN_USERNAME) + 
                   " / " + DEFAULT_ADMIN_PASSWORD;
        }
    }
}
