#include "file_handler.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

FileHandler::FileHandler(const std::string& static_root) : static_root_(static_root) {
    init_mime_types();
}

void FileHandler::init_mime_types() {
    mime_types_[".html"] = "text/html";
    mime_types_[".css"] = "text/css";
    mime_types_[".js"] = "application/javascript";
    mime_types_[".json"] = "application/json";
    mime_types_[".xml"] = "application/xml";
    mime_types_[".txt"] = "text/plain";
    mime_types_[".jpg"] = "image/jpeg";
    mime_types_[".jpeg"] = "image/jpeg";
    mime_types_[".png"] = "image/png";
    mime_types_[".gif"] = "image/gif";
    mime_types_[".svg"] = "image/svg+xml";
    // Add common MIME types
    mime_types_[".pdf"] = "application/pdf";
    mime_types_[".zip"] = "application/zip";
    mime_types_[".uacc"] = "application/json";
}

std::string FileHandler::get_mime_type(const std::string& file_extension) {
    auto it = mime_types_.find(file_extension);
    if (it != mime_types_.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

std::string FileHandler::get_file_extension(const std::string& file_path) {
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return file_path.substr(dot_pos);
    }
    return "";
}

HttpResponse FileHandler::serve_static_file(const HttpRequest& request, const std::string& relative_path) {
    HttpResponse response;
    
    // Construct full file path
    std::string full_path = static_root_ + "/" + relative_path;
    
    // Security check - prevent directory traversal
    if (relative_path.find("..") != std::string::npos) {
        response.set_error(403, "Forbidden");
        add_security_headers(response);
        return response;
    }
    
    // Check if file exists
    if (!file_exists(full_path)) {
        LOG_FILE_REQUEST(relative_path, false);
        response.set_error(404, "File not found");
        add_security_headers(response);
        return response;
    }
    
    // Check if it's a directory
    if (is_directory(full_path)) {
        // Try to serve index.html
        std::string index_path = full_path + "/index.html";
        if (file_exists(index_path)) {
            full_path = index_path;
        } else {
            return serve_directory_listing(request, relative_path);
        }
    }
    
    // Get file info
    std::string content_type = get_mime_type(get_file_extension(full_path));
    std::string last_modified = get_last_modified(full_path);
    
    // Set headers
    response.headers["Content-Type"] = content_type;
    response.headers["Last-Modified"] = last_modified;
    response.headers["Cache-Control"] = "public, max-age=3600";
    
    // Read file content
    std::ifstream file(full_path, std::ios::binary);
    if (file.is_open()) {
        response.body = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        file.close();
        LOG_FILE_REQUEST(relative_path, true);
    } else {
        response.set_error(500, "Failed to read file");
    }
    
    add_security_headers(response);
    return response;
}

HttpResponse FileHandler::handle_file_upload(const HttpRequest& request, const std::string& upload_dir) {
    HttpResponse response;
    
    // Check content type
    auto content_type_it = request.headers.find("Content-Type");
    if (content_type_it == request.headers.end()) {
        response.set_error(400, "Content-Type header is required");
        return response;
    }
    
    std::string content_type = content_type_it->second;
    if (content_type.substr(0, 19) != "multipart/form-data") {
        response.set_error(400, "Content-Type must be multipart/form-data");
        return response;
    }
    
    // Create upload directory if it doesn't exist
    if (!create_directory(upload_dir)) {
        response.set_error(500, "Failed to create upload directory");
        return response;
    }
    
    try {
        std::vector<FileInfo> uploaded_files = process_uploaded_files(request.body, content_type, upload_dir);
        
        nlohmann::json response_json;
        response_json["success"] = true;
        response_json["message"] = "Files uploaded successfully";
        response_json["data"]["files"] = nlohmann::json::array();
        response_json["data"]["total_size"] = 0;
        
        for (const auto& file : uploaded_files) {
            response_json["data"]["files"].push_back({
                {"name", file.name},
                {"size", file.size},
                {"type", file.content_type}
            });
            response_json["data"]["total_size"] = response_json["data"]["total_size"].get<long>() + file.size;
        }
        
        response.set_json_content(response_json.dump());
        
    } catch (const std::exception& e) {
        response.set_error(500, "Failed to process uploaded files");
    }
    
    add_security_headers(response);
    return response;
}

std::vector<FileInfo> FileHandler::process_uploaded_files(const std::string& body, const std::string& content_type, const std::string& upload_dir) {
    std::vector<FileInfo> uploaded_files;
    
    // Extract boundary from content type
    std::vector<std::string> boundary_parts = parse_multipart_boundary(content_type);
    if (boundary_parts.empty()) {
        return uploaded_files;
    }
    
    std::string boundary = boundary_parts[0];
    
    // Parse multipart data
    std::map<std::string, std::string> form_data = parse_multipart_data(body, boundary);
    
    // Process uploaded files
    for (const auto& [field_name, file_content] : form_data) {
        if (field_name == "files") {
            // Extract filename from content disposition
            std::regex filename_regex("filename=\"([^\"]*)\"");
            std::smatch filename_match;
            
            if (std::regex_search(body, filename_match, filename_regex)) {
                std::string filename = filename_match[1].str();
                
                if (!filename.empty()) {
                    // Generate unique filename
                    std::string unique_filename = generate_unique_filename(filename);
                    std::string saved_path = save_uploaded_file(unique_filename, file_content, upload_dir);
                    
                    if (!saved_path.empty()) {
                        FileInfo file_info;
                        file_info.name = filename;
                        file_info.path = saved_path;
                        file_info.size = file_content.length();
                        file_info.content_type = get_mime_type(get_file_extension(filename));
                        file_info.last_modified = get_last_modified(saved_path);
                        
                        uploaded_files.push_back(file_info);
                    }
                }
            }
        }
    }
    
    return uploaded_files;
}

std::vector<std::string> FileHandler::parse_multipart_boundary(const std::string& content_type) {
    std::vector<std::string> boundaries;
    
    std::regex boundary_regex(R"(boundary=([^;]+))");
    std::smatch boundary_match;
    
    if (std::regex_search(content_type, boundary_match, boundary_regex)) {
        boundaries.push_back("--" + boundary_match[1].str());
    }
    
    return boundaries;
}

std::map<std::string, std::string> FileHandler::parse_multipart_data(const std::string& body, const std::string& boundary) {
    std::map<std::string, std::string> form_data;
    
    size_t pos = 0;
    std::string delimiter = boundary + "\r\n";
    
    while ((pos = body.find(delimiter, pos)) != std::string::npos) {
        pos += delimiter.length();
        
        // Find end of headers
        size_t headers_end = body.find("\r\n\r\n", pos);
        if (headers_end == std::string::npos) break;
        
        // Extract headers
        std::string headers = body.substr(pos, headers_end - pos);
        
        // Extract field name from content disposition
        std::regex name_regex("name=\"([^\"]*)\"");
        std::smatch name_match;
        
        if (std::regex_search(headers, name_match, name_regex)) {
            std::string field_name = name_match[1].str();
            
            // Find end of this part
            size_t part_end = body.find(boundary, headers_end + 4);
            if (part_end == std::string::npos) break;
            
            // Extract content
            std::string content = body.substr(headers_end + 4, part_end - headers_end - 4);
            
            // Remove trailing \r\n if present
            if (content.length() >= 2 && content.substr(content.length() - 2) == "\r\n") {
                content = content.substr(0, content.length() - 2);
            }
            
            form_data[field_name] = content;
        }
        
        pos = headers_end;
    }
    
    return form_data;
}

std::string FileHandler::save_uploaded_file(const std::string& filename, const std::string& content, const std::string& upload_dir) {
    std::string full_path = upload_dir + "/" + filename;
    
    std::ofstream file(full_path, std::ios::binary);
    if (file.is_open()) {
        file.write(content.c_str(), content.length());
        file.close();
        return full_path;
    }
    
    return "";
}

std::string FileHandler::generate_unique_filename(const std::string& original_name) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::string extension = get_file_extension(original_name);
    std::string name_without_ext = original_name.substr(0, original_name.length() - extension.length());
    
    return name_without_ext + "_" + std::to_string(timestamp) + extension;
}

bool FileHandler::create_directory(const std::string& dir_path) {
    try {
        return fs::create_directories(dir_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to create directory: " << e.what() << std::endl;
        return false;
    }
}

bool FileHandler::file_exists(const std::string& file_path) {
    return fs::exists(file_path);
}

bool FileHandler::is_directory(const std::string& file_path) {
    return fs::is_directory(file_path);
}

std::string FileHandler::get_last_modified(const std::string& file_path) {
    try {
        auto ftime = fs::last_write_time(file_path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
        
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&cftime), "%a, %d %b %Y %H:%M:%S GMT");
        return ss.str();
    } catch (const std::exception& e) {
        return "";
    }
}

bool FileHandler::validate_file_size(size_t size, size_t max_size) {
    return size <= max_size;
}

bool FileHandler::validate_file_type(const std::string& filename, const std::vector<std::string>& allowed_types) {
    std::string extension = get_file_extension(filename);
    return std::find(allowed_types.begin(), allowed_types.end(), extension) != allowed_types.end();
}

void FileHandler::add_security_headers(HttpResponse& response) {
    response.headers["X-Content-Type-Options"] = "nosniff";
    response.headers["X-Frame-Options"] = "DENY";
    response.headers["X-XSS-Protection"] = "1; mode=block";
    response.headers["Strict-Transport-Security"] = "max-age=31536000; includeSubDomains";
    response.headers["Referrer-Policy"] = "strict-origin-when-cross-origin";
}

void FileHandler::add_cors_headers(HttpResponse& response, const std::string& origin) {
    response.headers["Access-Control-Allow-Origin"] = origin;
    response.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    response.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    response.headers["Access-Control-Max-Age"] = "86400";
}

std::string FileHandler::format_file_size(size_t size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size_d = static_cast<double>(size);
    
    while (size_d >= 1024.0 && unit_index < 4) {
        size_d /= 1024.0;
        unit_index++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size_d << " " << units[unit_index];
    return ss.str();
}

HttpResponse FileHandler::serve_directory_listing(const HttpRequest& request, const std::string& dir_path) {
    HttpResponse response;
    
    try {
        if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
            response.set_error(404, "Directory not found");
            return response;
        }
        
        // Generate HTML directory listing
        std::stringstream html;
        html << "<!DOCTYPE html>\n"
             << "<html>\n"
             << "<head><title>Directory Listing</title></head>\n"
             << "<body>\n"
             << "<h1>Directory Listing: " << dir_path << "</h1>\n"
             << "<ul>\n";
        
        // Add parent directory link
        if (dir_path != static_root_) {
            html << "<li><a href=\"../\">../</a></li>\n";
        }
        
        // List directory contents
        for (const auto& entry : fs::directory_iterator(dir_path)) {
            std::string name = entry.path().filename().string();
            if (entry.is_directory()) {
                html << "<li><strong><a href=\"" << name << "/\">" << name << "/</a></strong></li>\n";
            } else {
                html << "<li><a href=\"" << name << "\">" << name << "</a></li>\n";
            }
        }
        
        html << "</ul>\n"
             << "</body>\n"
             << "</html>";
        
        response.set_file_content(html.str(), "text/html");
        
    } catch (const std::exception& e) {
        response.set_error(500, std::string("Error reading directory: ") + e.what());
    }
    
    return response;
}
