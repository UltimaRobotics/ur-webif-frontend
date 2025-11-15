#include "http_server.h"
#include "file_handler.h"
#include "logger.h"

#ifdef HAVE_MICROHTTPD
#include <microhttpd.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <arpa/inet.h>
#endif

#include <fstream>
#include <nlohmann/json.hpp>

// Helper function to iterate over headers
struct HeaderIteratorData {
    std::map<std::string, std::string>* headers;
};

#ifdef HAVE_MICROHTTPD
static enum MHD_Result header_iterator(void* cls, enum MHD_ValueKind kind,
                                       const char* key, const char* value) {
    HeaderIteratorData* data = static_cast<HeaderIteratorData*>(cls);
    if (data && data->headers && key && value) {
        (*data->headers)[std::string(key)] = std::string(value);
    }
    return MHD_YES;
}

static enum MHD_Result get_arg_iterator(void* cls, enum MHD_ValueKind kind,
                                        const char* key, const char* value) {
    std::map<std::string, std::string>* params = static_cast<std::map<std::string, std::string>*>(cls);
    if (params && key && value) {
        (*params)[std::string(key)] = std::string(value);
    }
    return MHD_YES;
}
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sstream>
#include <iostream>
#include <regex>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

HttpServer::HttpServer(const std::string& host, int port, int max_connections, int thread_pool_size)
    : host_(host), port_(port), running_(false), max_connections_(max_connections), thread_pool_size_(thread_pool_size)
#ifdef HAVE_MICROHTTPD
    , daemon_(nullptr)
#endif
{
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
#ifdef HAVE_MICROHTTPD
    if (running_) {
        return true;
    }
    
    // Start libmicrohttpd daemon
    unsigned int flags = MHD_USE_SELECT_INTERNALLY | MHD_USE_ERROR_LOG;
    
    daemon_ = MHD_start_daemon(flags, port_, nullptr, nullptr,
                              &HttpServer::access_handler_callback, this,
                              MHD_OPTION_CONNECTION_LIMIT, max_connections_,
                              MHD_OPTION_CONNECTION_TIMEOUT, 120,
                              MHD_OPTION_END);
    
    if (!daemon_) {
        std::cerr << "Failed to start HTTP server with libmicrohttpd" << std::endl;
        return false;
    }
    
    running_ = true;
    return true;
#else
    std::cerr << "libmicrohttpd not available" << std::endl;
    return false;
#endif
}

void HttpServer::stop() {
#ifdef HAVE_MICROHTTPD
    if (running_ && daemon_) {
        running_ = false;
        MHD_stop_daemon(daemon_);
        daemon_ = nullptr;
    }
#endif
}

// Connection context for handling POST data
struct ConnectionContext {
    std::string post_data;
    bool first_call;
    
    ConnectionContext() : first_call(true) {}
};

#ifdef HAVE_MICROHTTPD
enum MHD_Result HttpServer::access_handler_callback(void* cls,
                                                    struct MHD_Connection* connection,
                                                    const char* url,
                                                    const char* method,
                                                    const char* version,
                                                    const char* upload_data,
                                                    size_t* upload_data_size,
                                                    void** con_cls) {
    HttpServer* server = static_cast<HttpServer*>(cls);
    if (!server) {
        return MHD_NO;
    }
    
    // Initialize connection context on first call
    if (*con_cls == nullptr) {
        std::string* post_data = new std::string();
        *con_cls = post_data;
        return MHD_YES;
    }
    
    std::string* post_data = static_cast<std::string*>(*con_cls);
    if (!post_data) {
        return MHD_NO;
    }
    
    // Handle POST data upload
    if (*upload_data_size > 0) {
        post_data->append(upload_data, *upload_data_size);
        *upload_data_size = 0; // Indicate we've processed all data
        return MHD_YES;
    }
    
    // Final call - process the complete request
    struct MHD_Response* mhd_response = nullptr;
    enum MHD_Result result = MHD_NO;
    
    try {
        // Convert MHD request to our HttpRequest format
        HttpRequest request;
        std::cerr << "DEBUG: POST data length: " << post_data->length() << std::endl;
        std::cerr << "DEBUG: POST data content: " << *post_data << std::endl;
        server->convert_mhd_request(url, method, connection, post_data->c_str(), post_data->length(), request);
        std::cerr << "DEBUG: Request body after conversion: " << request.body << std::endl;
        
        // Log HTTP request in verbose mode
        LOG_HTTP_REQUEST(request.method, request.path, request.client_ip);
        
        // Find matching route
        HttpResponse response;
        bool route_found = false;
        
        auto method_routes = server->routes_.find(request.method);
        if (method_routes != server->routes_.end()) {
            auto route_handler = method_routes->second.find(request.path);
            if (route_handler != method_routes->second.end()) {
                std::cerr << "DEBUG: Found route handler for " << method << " " << url << std::endl;
                try {
                    // Check if we can access the route handler function safely
                    std::cerr << "DEBUG: About to call route handler" << std::endl;
                    response = route_handler->second(request);
                    route_found = true;
                    std::cerr << "DEBUG: Route handler completed successfully" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "DEBUG: Route handler threw std::exception: " << e.what() << std::endl;
                    response.set_error(500, "Route handler exception");
                    route_found = true;
                } catch (...) {
                    std::cerr << "DEBUG: Route handler threw unknown exception" << std::endl;
                    response.set_error(500, "Route handler unknown exception");
                    route_found = true;
                }
            }
            
            // If no exact match found, try wildcard matching
            if (!route_found) {
                for (const auto& route_pair : method_routes->second) {
                    const std::string& route_pattern = route_pair.first;
                    if (route_pattern.length() >= 2 && route_pattern.substr(route_pattern.length() - 2) == "/*") {
                        std::string prefix = route_pattern.substr(0, route_pattern.length() - 2);
                        if (request.path.length() >= prefix.length() && 
                            request.path.substr(0, prefix.length()) == prefix) {
                            std::cerr << "DEBUG: Found wildcard route handler for " << method << " " << url << " matching pattern " << route_pattern << std::endl;
                            try {
                                response = route_pair.second(request);
                                route_found = true;
                                std::cerr << "DEBUG: Wildcard route handler completed successfully" << std::endl;
                                break;
                            } catch (const std::exception& e) {
                                std::cerr << "DEBUG: Wildcard route handler threw std::exception: " << e.what() << std::endl;
                                response.set_error(500, "Wildcard route handler exception");
                                route_found = true;
                                break;
                            } catch (...) {
                                std::cerr << "DEBUG: Wildcard route handler threw unknown exception" << std::endl;
                                response.set_error(500, "Wildcard route handler unknown exception");
                                route_found = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        if (!route_found) {
            std::cerr << "DEBUG: No route found for " << method << " " << url << std::endl;
            response.set_error(404, "Not Found");
        }
        
        // Log HTTP response in verbose mode
        LOG_HTTP_RESPONSE(response.status_code, response.body.length());
        
        // Create and queue MHD response
        std::cerr << "DEBUG: Creating MHD response" << std::endl;
        mhd_response = server->create_mhd_response(response);
        if (!mhd_response) {
            std::cerr << "DEBUG: Failed to create MHD response" << std::endl;
            delete post_data;
            *con_cls = nullptr;
            return MHD_NO;
        }
        
        std::cerr << "DEBUG: Queuing MHD response" << std::endl;
        result = MHD_queue_response(connection, response.status_code, mhd_response);
        std::cerr << "DEBUG: Destroying MHD response" << std::endl;
        MHD_destroy_response(mhd_response);
        std::cerr << "DEBUG: Response handling completed" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling request: " << e.what() << std::endl;
        
        HttpResponse error_response;
        error_response.set_error(500, "Internal Server Error");
        
        // Log error response in verbose mode
        LOG_HTTP_RESPONSE(500, 0);
        
        mhd_response = server->create_mhd_response(error_response);
        if (mhd_response) {
            result = MHD_queue_response(connection, 500, mhd_response);
            MHD_destroy_response(mhd_response);
        }
    }
    
    // Clean up connection context
    delete post_data;
    *con_cls = nullptr;
    
    return result;
}

void HttpServer::convert_mhd_request(const char* url, const char* method,
                                     struct MHD_Connection* connection,
                                     const char* upload_data, size_t upload_data_size,
                                     HttpRequest& request) {
    // Basic request info
    request.method = std::string(method);
    request.path = std::string(url);
    
    // Parse query string from URL
    std::string url_str(url);
    size_t query_pos = url_str.find('?');
    if (query_pos != std::string::npos) {
        request.query_string = url_str.substr(query_pos + 1);
        request.path = url_str.substr(0, query_pos);
        
        // Parse query parameters
        MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, get_arg_iterator, &request.query_params);
    }
    
    // Get headers
    HeaderIteratorData header_data;
    header_data.headers = &request.headers;
    MHD_get_connection_values(connection, MHD_HEADER_KIND, header_iterator, &header_data);
    
    // Get content length from headers
    size_t content_length = 0;
    auto content_length_it = request.headers.find("Content-Length");
    if (content_length_it != request.headers.end()) {
        try {
            content_length = std::stoull(content_length_it->second);
        } catch (const std::exception&) {
            content_length = 0;
        }
    }
    
    // Get body if present (use the accumulated POST data)
    if (upload_data && upload_data_size > 0) {
        request.body = std::string(upload_data, upload_data_size);
    }
    
    // Get client IP
    const union MHD_ConnectionInfo* conn_info = MHD_get_connection_info(connection, MHD_CONNECTION_INFO_CLIENT_ADDRESS);
    if (conn_info) {
        struct sockaddr* addr = const_cast<struct sockaddr*>(conn_info->client_addr);
        if (addr->sa_family == AF_INET) {
            struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(addr);
            request.client_ip = inet_ntoa(addr_in->sin_addr);
        }
    }
}

struct MHD_Response* HttpServer::create_mhd_response(const HttpResponse& response) {
    // Create response from body
    struct MHD_Response* mhd_response = MHD_create_response_from_buffer(
        response.body.length(),
        const_cast<char*>(response.body.c_str()),
        MHD_RESPMEM_MUST_COPY
    );
    
    if (!mhd_response) {
        return nullptr;
    }
    
    // Add headers
    add_response_headers(mhd_response, response);
    
    return mhd_response;
}

void HttpServer::add_response_headers(struct MHD_Response* mhd_response, const HttpResponse& response) {
    for (const auto& header : response.headers) {
        MHD_add_response_header(mhd_response, header.first.c_str(), header.second.c_str());
    }
}
#endif

void HttpServer::get(const std::string& path, RouteHandler handler) {
    routes_["GET"][path] = handler;
}

void HttpServer::post(const std::string& path, RouteHandler handler) {
    routes_["POST"][path] = handler;
}

void HttpServer::put(const std::string& path, RouteHandler handler) {
    routes_["PUT"][path] = handler;
}

void HttpServer::del(const std::string& path, RouteHandler handler) {
    routes_["DELETE"][path] = handler;
}

void HttpServer::options(const std::string& path, RouteHandler handler) {
    routes_["OPTIONS"][path] = handler;
}

void HttpServer::serve_static_files(const std::string& url_prefix, const std::string& file_system_path) {
    // Register a catch-all handler for the URL prefix
    get(url_prefix + "/*", [this, file_system_path, url_prefix](const HttpRequest& request) {
        // Extract the requested path from the request
        std::string requested_path = request.path.substr(url_prefix.length());
        
        // Prevent directory traversal attacks
        if (requested_path.find("..") != std::string::npos) {
            HttpResponse response;
            response.set_error(403, "Access denied");
            return response;
        }
        
        // Construct the full file system path
        std::string full_path = file_system_path + requested_path;
        
        // Create a FileHandler to serve the file
        FileHandler file_handler(file_system_path);
        return file_handler.serve_static_file(request, requested_path);
    });
}

void HttpServer::enable_cors(const std::vector<std::string>& allowed_origins,
                           const std::vector<std::string>& allowed_methods,
                           const std::vector<std::string>& allowed_headers) {
    // This would need to be implemented to add CORS headers to responses
    // For now, the CORS handling is done in the individual route handlers
}

// HttpResponse methods
void HttpResponse::set_json_content(const std::string& json_data) {
    headers["Content-Type"] = "application/json";
    body = json_data;
}

void HttpResponse::set_file_content(const std::string& file_path, const std::string& content_type) {
    if (!content_type.empty()) {
        headers["Content-Type"] = content_type;
    }
    
    std::ifstream file(file_path, std::ios::binary);
    if (file.is_open()) {
        body = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        file.close();
    } else {
        set_error(404, "File not found");
    }
}

void HttpResponse::set_error(int status_code, const std::string& message) {
    this->status_code = status_code;
    headers["Content-Type"] = "application/json";
    
    nlohmann::json error_json;
    error_json["success"] = false;
    error_json["message"] = message;
    error_json["status_code"] = status_code;
    
    body = error_json.dump();
}
