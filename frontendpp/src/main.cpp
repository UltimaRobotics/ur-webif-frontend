#include "config_manager.h"
#include "http_server.h"
#include "auth_handler.h"
#include "jwt_manager.h"
#include "file_handler.h"
#include "logger.h"
#include "frontendpp/cmake/attributes.hpp"
#include <iostream>
#include <memory>
#include <csignal>
#include <unistd.h>
#include <filesystem>
#include <fstream>

std::unique_ptr<HttpServer> server;
std::unique_ptr<ConfigManager> config_manager;
std::unique_ptr<AuthHandler> auth_handler;
std::unique_ptr<FileHandler> file_handler;
std::unique_ptr<JWTManager> jwt_manager;

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down server..." << std::endl;
    if (server) {
        server->stop();
    }
    exit(0);
}

void validate_file_serving(const PathsConfig& paths_config) {
    std::cout << "🔍 Validating file serving configuration..." << std::endl;
    
    namespace fs = std::filesystem;
    
    // Check frontend root directory
    if (!fs::exists(paths_config.frontend_root)) {
        std::cerr << "❌ Frontend root directory does not exist: " << paths_config.frontend_root << std::endl;
        return;
    }
    
    std::cout << "✅ Frontend root directory: " << fs::absolute(paths_config.frontend_root) << std::endl;
    
    // Check critical files
    std::vector<std::string> critical_files = {
        "login-page.html",
        "source.html"
    };
    
    for (const auto& file : critical_files) {
        std::string file_path = paths_config.frontend_root + "/" + file;
        if (fs::exists(file_path)) {
            auto file_size = fs::file_size(file_path);
            std::cout << "✅ Found " << file << " (" << file_size << " bytes)" << std::endl;
        } else {
            std::cerr << "❌ Missing critical file: " << file << std::endl;
        }
    }
    
    // Check static files directory
    if (!fs::exists(paths_config.static_files)) {
        std::cout << "⚠️  Static files directory does not exist: " << paths_config.static_files << std::endl;
    } else {
        int file_count = 0;
        for (const auto& entry : fs::recursive_directory_iterator(paths_config.static_files)) {
            if (entry.is_regular_file()) {
                file_count++;
            }
        }
        std::cout << "✅ Static files directory: " << file_count << " files found" << std::endl;
    }
    
    // Check templates directory
    if (!fs::exists(paths_config.templates)) {
        std::cout << "⚠️  Templates directory does not exist: " << paths_config.templates << std::endl;
    } else {
        int template_count = 0;
        for (const auto& entry : fs::directory_iterator(paths_config.templates)) {
            if (entry.is_regular_file()) {
                template_count++;
            }
        }
        std::cout << "✅ Templates directory: " << template_count << " templates found" << std::endl;
    }
}

void setup_routes() {
    auto server_config = config_manager->get_server_config();
    auto paths_config = config_manager->get_paths_config();
    auto security_config = config_manager->get_security_config();
    
    // Root route handler with authentication-based redirection
    server->get("/", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        HttpResponse response;
        
        // Check if user is authenticated
        UserInfo user_info;
        if (auth_handler->authenticate_request(request, user_info)) {
            // User is authenticated - redirect to source.html
            response.status_code = 302;
            response.headers["Location"] = "/source.html";
        } else {
            // User is not authenticated - redirect to login-page.html
            response.status_code = 302;
            response.headers["Location"] = "/login-page.html";
        }
        
        return response;
    });
    
    // Authentication routes
    server->post("/api/auth/login", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        std::cerr << "DEBUG: auth_handler pointer: " << (void*)auth_handler << std::endl;
        if (!auth_handler) {
            HttpResponse error_response;
            error_response.set_error(500, "Auth handler is NULL");
            return error_response;
        }
        return auth_handler->handle_login(request);
    });
    
    server->post("/api/auth/login-with-key", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_login_with_key(request);
    });
    
    server->post("/api/auth/refresh", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_refresh_token(request);
    });
    
    server->post("/api/auth/logout", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_logout(request);
    });
    
    server->post("/api/auth/verify", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_refresh_token(request);
    });
    
    server->get("/api/auth/user", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_get_user_info(request);
    });
    
    server->get("/api/auth/validate", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_validate_token(request);
    });
    
    server->post("/api/auth/change-password", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_change_password(request);
    });
    
    server->post("/api/auth/generate-key", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_generate_auth_key(request);
    });
    
    server->get("/api/auth/list-keys", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_list_auth_keys(request);
    });
    
    server->post("/api/auth/revoke-key", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_revoke_auth_key(request);
    });
    
    server->post("/api/auth/upload-files", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_upload_files(request);
    });
    
    server->get("/api/auth/transfer-history", [auth_handler = auth_handler.get()](const HttpRequest& request) {
        return auth_handler->handle_get_transfer_history(request);
    });
    
    // Static file serving
    server->serve_static_files("/", paths_config.frontend_root);
    
    // CORS preflight handler with production security headers
    server->options("/*", [security_config](const HttpRequest& request) {
        HttpResponse response;
        response.status_code = 200;
        
        auto origin_it = request.headers.find("Origin");
        std::string origin = origin_it != request.headers.end() ? origin_it->second : "*";
        
        response.headers["Access-Control-Allow-Origin"] = origin;
        response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
        response.headers["Access-Control-Max-Age"] = "86400";
        response.headers["Content-Length"] = "0";
        
        // Add production security headers if enabled
        if (security_config.enable_security_headers) {
            response.headers["X-Frame-Options"] = "DENY";
            response.headers["X-Content-Type-Options"] = "nosniff";
            response.headers["X-XSS-Protection"] = "1; mode=block";
            response.headers["Referrer-Policy"] = "strict-origin-when-cross-origin";
            response.headers["Strict-Transport-Security"] = security_config.strict_transport_security;
            response.headers["Content-Security-Policy"] = security_config.content_security_policy;
        }
        
        return response;
    });
}

void print_startup_info() {
    auto server_config = config_manager->get_server_config();
    auto paths_config = config_manager->get_paths_config();
    auto auth_config = config_manager->get_auth_config();
    auto security_config = config_manager->get_security_config();
    
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║     UR WebIF Frontend++ C++ Authentication Server          ║
╠══════════════════════════════════════════════════════════════╣
║  Mode:         C++ HTTP Server with JWT Auth               ║
║  Host:         )" << server_config.host << R"(
║  Port:         )" << server_config.port << R"(
║  Max Connections: )" << server_config.max_connections << R"(
║  Thread Pool:  )" << server_config.thread_pool_size << R"(
║  Frontend Root: )" << paths_config.frontend_root << R"(
║  Static Files: )" << paths_config.static_files << R"(
║  JWT Expiry:   )" << auth_config.token_expiry_hours << R"( hours
║  CORS:         )" << (security_config.enable_cors ? "Enabled" : "Disabled") << R"(
╚══════════════════════════════════════════════════════════════╝

🔐 Authentication endpoints:
   POST /api/auth/login           - User login
   POST /api/auth/login-with-key  - Login with auth key
   POST /api/auth/refresh         - Refresh token
   POST /api/auth/logout          - User logout
   POST /api/auth/verify          - Verify token
   GET  /api/auth/user            - Get user info
   POST /api/auth/change-password - Change password
   POST /api/auth/generate-key    - Generate auth key
   GET  /api/auth/list-keys       - List auth keys
   POST /api/auth/revoke-key      - Revoke auth key
   POST /api/auth/upload-files    - Upload files
   GET  /api/auth/transfer-history - Transfer history

👤 Default credentials:
   • admin / admin123

🚀 Open http://)" << server_config.host << ":" << server_config.port << R"(/login-page.html in your browser
📝 Configuration: )" << (getenv("PKG_CONFIG") ? getenv("PKG_CONFIG") : "config/server.json") << R"(
)";

}

int main(int argc, char* argv[]) {
    std::cout << "=== Frontend++ Starting ===" << std::endl;
    
    // Parse command line arguments
    std::string config_path = "config/server.json";
    bool verbose_mode = false;
    
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-pkg_config" && i + 1 < argc) {
            config_path = argv[i + 1];
            i++;
        } else if (std::string(argv[i]) == "-v" || std::string(argv[i]) == "--verbose") {
            verbose_mode = true;
        } else if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            std::cout << "Usage: " << argv[0] << " [-pkg_config <config_file_path>] [-v|--verbose] [--help|-h]\n";
            std::cout << "  -pkg_config  Path to JSON configuration file (default: config/server.json)\n";
            std::cout << "  -v, --verbose Enable verbose logging mode\n";
            std::cout << "  --help, -h   Show this help message\n";
            return 0;
        }
    }
    
    // Initialize logger first
    auto& logger = FrontendPP::Logger::get_instance();
    logger.set_verbose_mode(verbose_mode);
    
    if (verbose_mode) {
        logger.enable_file_logging("logs/frontendpp.log");
        LOG_INFO(FrontendPP::BuildAttributes::get_build_info());
        LOG_INFO("Verbose logging mode enabled");
        LOG_INFO("Log file: logs/frontendpp.log");
    }
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize configuration manager
    config_manager = std::make_unique<ConfigManager>();
    if (!config_manager->load_config(config_path)) {
        std::cerr << "Failed to load configuration from: " << config_path << std::endl;
        return 1;
    }
    
    auto server_config = config_manager->get_server_config();
    auto auth_config = config_manager->get_auth_config();
    auto database_config = config_manager->get_database_config();
    auto paths_config = config_manager->get_paths_config();
    
    // Validate and fix JWT secret before creating JWTManager
    std::string validated_jwt_secret = auth_config.jwt_secret;
    if (validated_jwt_secret == FrontendPP::BuildAttributes::DEFAULT_JWT_SECRET || 
        validated_jwt_secret.empty() || validated_jwt_secret.length() < 32) {
        LOG_INFO("JWT secret is invalid or default, generating new one");
        validated_jwt_secret = AuthHandler::generate_jwt_secret();
        if (!AuthHandler::update_config_jwt_secret(config_path, validated_jwt_secret)) {
            LOG_WARNING("Failed to update JWT secret in config file, using generated secret in memory");
        }
    }
    
    // Initialize JWT manager with validated secret
    jwt_manager = std::make_unique<JWTManager>(validated_jwt_secret, 
                                              auth_config.issuer, 
                                              auth_config.audience,
                                              auth_config.token_expiry_minutes,
                                              auth_config.refresh_token_expiry_minutes,
                                              auth_config.enable_sliding_expiration,
                                              auth_config.token_refresh_threshold_minutes);
    
    // Initialize auth handler
    auth_handler = std::make_unique<AuthHandler>(database_config.path, *jwt_manager);
    
    // Enhanced initialization: validate database integrity
    if (!auth_handler->validate_database_integrity()) {
        LOG_CRITICAL("Database integrity validation failed. Server cannot start safely.");
        return 1;
    }
    
    // Initialize file handler
    file_handler = std::make_unique<FileHandler>(paths_config.static_files);
    
    // Enhanced initialization: validate file serving configuration
    validate_file_serving(paths_config);
    
    // Check critical files exist
    namespace fs = std::filesystem;
    std::vector<std::string> critical_files = {
        paths_config.frontend_root + "/login-page.html",
        paths_config.frontend_root + "/source.html"
    };
    
    for (const auto& file : critical_files) {
        if (!fs::exists(file)) {
            std::cerr << "❌ Critical file missing: " << file << std::endl;
            std::cerr << "❌ Server cannot start without required files." << std::endl;
            return 1;
        }
    }
    
    std::cout << "✅ All critical files validated successfully" << std::endl;
    
    // Initialize HTTP server
    server = std::make_unique<HttpServer>(server_config.host, 
                                         server_config.port,
                                         server_config.max_connections,
                                         server_config.thread_pool_size);
    
    // Setup routes
    setup_routes();
    
    // Print startup information
    print_startup_info();
    
    // Start server
    if (!server->start()) {
        std::cerr << "Failed to start HTTP server" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Frontend++ server started successfully!" << std::endl;
    std::cout << "🌐 Listening on http://" << server_config.host << ":" << server_config.port << std::endl;
    std::cout << "📁 Serving files from: " << paths_config.frontend_root << std::endl;
    std::cout << "\nPress Ctrl+C to stop the server\n";
    std::cout << "=" << std::string(64, '=') << std::endl;
    
    // Keep server running
    while (server->is_running()) {
        sleep(1);
    }
    
    std::cout << "\n🛑 Frontend++ server stopped" << std::endl;
    return 0;
}
