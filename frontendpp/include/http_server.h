#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <vector>
#include <atomic>

#ifdef HAVE_MICROHTTPD
#include <microhttpd.h>
#endif

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query_string;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
    std::string body;
    std::string client_ip;
};

struct HttpResponse {
    int status_code = 200;
    std::map<std::string, std::string> headers;
    std::string body;
    
    void set_json_content(const std::string& json_data);
    void set_file_content(const std::string& file_path, const std::string& content_type = "");
    void set_error(int status_code, const std::string& message);
};

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
private:
    std::string host_;
    int port_;
    std::atomic<bool> running_;
    int max_connections_;
    int thread_pool_size_;
    
#ifdef HAVE_MICROHTTPD
    struct MHD_Daemon* daemon_;
#endif
    
    std::map<std::string, std::map<std::string, RouteHandler>> routes_;
    
    // libmicrohttpd callback functions
    static enum MHD_Result access_handler_callback(void* cls,
                                                   struct MHD_Connection* connection,
                                                   const char* url,
                                                   const char* method,
                                                   const char* version,
                                                   const char* upload_data,
                                                   size_t* upload_data_size,
                                                   void** con_cls);
    
    // Helper functions for libmicrohttpd
    void convert_mhd_request(const char* url, const char* method, 
                             struct MHD_Connection* connection,
                             const char* upload_data, size_t upload_data_size,
                             HttpRequest& request);
    struct MHD_Response* create_mhd_response(const HttpResponse& response);
    void add_response_headers(struct MHD_Response* mhd_response, const HttpResponse& response);
    
public:
    HttpServer(const std::string& host, int port, int max_connections = 1000, int thread_pool_size = 4);
    ~HttpServer();
    
    // Server control
    bool start();
    void stop();
    bool is_running() const { return running_; }
    
    // Route registration
    void get(const std::string& path, RouteHandler handler);
    void post(const std::string& path, RouteHandler handler);
    void put(const std::string& path, RouteHandler handler);
    void del(const std::string& path, RouteHandler handler);
    void options(const std::string& path, RouteHandler handler);
    
    // Static file serving
    void serve_static_files(const std::string& url_prefix, const std::string& file_system_path);
    
    // CORS support
    void enable_cors(const std::vector<std::string>& allowed_origins,
                    const std::vector<std::string>& allowed_methods,
                    const std::vector<std::string>& allowed_headers);
};
