#pragma once

#include "http_server.h"
#include <string>
#include <map>
#include <vector>
#include <fstream>

struct FileInfo {
    std::string name;
    std::string path;
    size_t size;
    std::string content_type;
    std::string last_modified;
};

class FileHandler {
private:
    std::string static_root_;
    std::map<std::string, std::string> mime_types_;
    
    void init_mime_types();
    std::string get_mime_type(const std::string& file_extension);
    std::string get_file_extension(const std::string& file_path);
    std::string format_file_size(size_t size);
    std::string get_last_modified(const std::string& file_path);
    bool file_exists(const std::string& file_path);
    bool is_directory(const std::string& file_path);
    
    // File upload handling
    std::vector<std::string> parse_multipart_boundary(const std::string& content_type);
    std::map<std::string, std::string> parse_multipart_data(const std::string& body, const std::string& boundary);
    std::string save_uploaded_file(const std::string& filename, const std::string& content, const std::string& upload_dir);
    
public:
    FileHandler(const std::string& static_root);
    ~FileHandler() = default;
    
    // Static file serving
    HttpResponse serve_static_file(const HttpRequest& request, const std::string& relative_path);
    HttpResponse serve_directory_listing(const HttpRequest& request, const std::string& relative_path);
    
    // File upload handling
    HttpResponse handle_file_upload(const HttpRequest& request, const std::string& upload_dir = "uploads/");
    std::vector<FileInfo> process_uploaded_files(const std::string& body, const std::string& content_type, const std::string& upload_dir);
    
    // Utility methods
    bool validate_file_size(size_t size, size_t max_size);
    bool validate_file_type(const std::string& filename, const std::vector<std::string>& allowed_types);
    std::string generate_unique_filename(const std::string& original_name);
    bool create_directory(const std::string& dir_path);
    bool delete_file(const std::string& file_path);
    
    // CORS and security headers
    void add_security_headers(HttpResponse& response);
    void add_cors_headers(HttpResponse& response, const std::string& origin = "*");
};
