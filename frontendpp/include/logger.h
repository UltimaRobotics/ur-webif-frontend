#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace FrontendPP {
    enum class LogLevel {
        DEBUG = 0,
        INFO = 1,
        WARNING = 2,
        ERROR = 3,
        CRITICAL = 4
    };

    class Logger {
    private:
        static Logger* instance_;
        static std::mutex mutex_;
        bool verbose_mode_;
        bool file_logging_;
        std::ofstream log_file_;
        LogLevel min_level_;
        std::mutex log_mutex_;

        Logger() : verbose_mode_(false), file_logging_(false), min_level_(LogLevel::INFO) {}

        std::string get_timestamp() const {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            return ss.str();
        }

        std::string level_to_string(LogLevel level) const {
            switch (level) {
                case LogLevel::DEBUG:    return "DEBUG";
                case LogLevel::INFO:     return "INFO";
                case LogLevel::WARNING:  return "WARN";
                case LogLevel::ERROR:    return "ERROR";
                case LogLevel::CRITICAL: return "CRITICAL";
                default:                 return "UNKNOWN";
            }
        }

    public:
        // Delete copy constructor and assignment operator
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        static Logger& get_instance() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (instance_ == nullptr) {
                instance_ = new Logger();
            }
            return *instance_;
        }

        void set_verbose_mode(bool enabled) {
            std::lock_guard<std::mutex> lock(log_mutex_);
            verbose_mode_ = enabled;
            // Don't log here to avoid deadlock - caller can log separately if needed
        }

        void enable_file_logging(const std::string& filename) {
            std::lock_guard<std::mutex> lock(log_mutex_);
            log_file_.open(filename, std::ios::app);
            file_logging_ = log_file_.is_open();
            // Don't log here to avoid deadlock - caller can log separately if needed
            if (!file_logging_) {
                std::cerr << "Failed to open log file: " << filename << std::endl;
            }
        }

        void set_min_level(LogLevel level) {
            std::lock_guard<std::mutex> lock(log_mutex_);
            min_level_ = level;
        }

        void log(LogLevel level, const std::string& message) {
            if (level < min_level_) return;

            std::lock_guard<std::mutex> lock(log_mutex_);
            std::string timestamp = get_timestamp();
            std::string level_str = level_to_string(level);
            std::string log_entry = "[" + timestamp + "] [" + level_str + "] " + message;

            // Always log to console if verbose mode is enabled or level is high enough
            if (verbose_mode_ || level >= LogLevel::WARNING) {
                std::cout << log_entry << std::endl;
            }

            // Log to file if enabled
            if (file_logging_ && log_file_.is_open()) {
                log_file_ << log_entry << std::endl;
                log_file_.flush();
            }
        }

        void log_http_request(const std::string& method, const std::string& path, const std::string& client_ip) {
            if (verbose_mode_) {
                log(LogLevel::DEBUG, "HTTP " + method + " " + path + " from " + client_ip);
            }
        }

        void log_http_response(int status_code, size_t content_length) {
            if (verbose_mode_) {
                log(LogLevel::DEBUG, "HTTP Response: " + std::to_string(status_code) + 
                    " (" + std::to_string(content_length) + " bytes)");
            }
        }

        void log_file_request(const std::string& file_path, bool found) {
            if (verbose_mode_) {
                if (found) {
                    log(LogLevel::INFO, "File served: " + file_path);
                } else {
                    log(LogLevel::WARNING, "File not found: " + file_path);
                }
            }
        }

        void log_database_operation(const std::string& operation, bool success) {
            if (verbose_mode_) {
                if (success) {
                    log(LogLevel::DEBUG, "Database operation: " + operation + " - SUCCESS");
                } else {
                    log(LogLevel::ERROR, "Database operation: " + operation + " - FAILED");
                }
            }
        }

        void log_authentication_event(const std::string& event, const std::string& username, bool success) {
            if (verbose_mode_) {
                std::string status = success ? "SUCCESS" : "FAILED";
                log(LogLevel::INFO, "Auth " + event + " for user '" + username + "' - " + status);
            }
        }

        void log_initialization_step(const std::string& step, bool success) {
            if (verbose_mode_) {
                if (success) {
                    log(LogLevel::INFO, "✅ " + step);
                } else {
                    log(LogLevel::ERROR, "❌ " + step);
                }
            }
        }

        ~Logger() {
            if (log_file_.is_open()) {
                log_file_.close();
            }
        }
    };

    // Convenience macros for logging
    #define LOG_DEBUG(message)    FrontendPP::Logger::get_instance().log(FrontendPP::LogLevel::DEBUG, message)
    #define LOG_INFO(message)     FrontendPP::Logger::get_instance().log(FrontendPP::LogLevel::INFO, message)
    #define LOG_WARNING(message)  FrontendPP::Logger::get_instance().log(FrontendPP::LogLevel::WARNING, message)
    #define LOG_ERROR(message)    FrontendPP::Logger::get_instance().log(FrontendPP::LogLevel::ERROR, message)
    #define LOG_CRITICAL(message) FrontendPP::Logger::get_instance().log(FrontendPP::LogLevel::CRITICAL, message)

    #define LOG_HTTP_REQUEST(method, path, ip) \
        FrontendPP::Logger::get_instance().log_http_request(method, path, ip)
    #define LOG_HTTP_RESPONSE(status, size) \
        FrontendPP::Logger::get_instance().log_http_response(status, size)
    #define LOG_FILE_REQUEST(path, found) \
        FrontendPP::Logger::get_instance().log_file_request(path, found)
    #define LOG_DATABASE_OPERATION(op, success) \
        FrontendPP::Logger::get_instance().log_database_operation(op, success)
    #define LOG_AUTH_EVENT(event, user, success) \
        FrontendPP::Logger::get_instance().log_authentication_event(event, user, success)
    #define LOG_INIT_STEP(step, success) \
        FrontendPP::Logger::get_instance().log_initialization_step(step, success)
}
