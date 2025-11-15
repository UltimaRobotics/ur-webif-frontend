#ifndef SYSTEM_DATA_COLLECTOR_H
#define SYSTEM_DATA_COLLECTOR_H

#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class SystemDataCollector {
public:
    struct SystemMetrics {
        // CPU Metrics
        struct CPU {
            double usage_percent = 0.0;
            int cores = 0;
            double temperature_celsius = 0.0;
            double frequency_ghz = 0.0;
        } cpu;
        
        // RAM Metrics
        struct RAM {
            double usage_percent = 0.0;
            double used_gb = 0.0;
            double total_gb = 0.0;
        } ram;
        
        // Swap Metrics
        struct Swap {
            double usage_percent = 0.0;
            double used_mb = 0.0;
            double total_gb = 0.0;
            std::string status = "Normal";
        } swap;
        
        // Network Metrics
        struct Network {
            struct Internet {
                std::string status = "Unknown";
                std::string external_ip = "N/A";
                std::string dns_primary = "N/A";
                std::string dns_secondary = "N/A";
                double latency_ms = 0.0;
                std::string bandwidth = "N/A";
            } internet;
            
            struct Connection {
                std::string status = "Unknown";
                std::string interface_name = "N/A";
                std::string mac_address = "N/A";
                std::string local_ip = "N/A";
                std::string gateway = "N/A";
                std::string speed = "N/A";
            } connection;
        } network;
        
        // Ultima Server Metrics (placeholder)
        struct UltimaServer {
            std::string status = "Unknown";
            std::string server = "N/A";
            int port = 0;
            std::string protocol = "N/A";
            double last_ping_ms = 0.0;
            std::string session = "N/A";
        } ultima_server;
        
        // Signal Metrics (cellular modem specific)
        struct Signal {
            struct Strength {
                std::string status = "No Signal";
                double rssi_dbm = 0.0;
                double rsrp_dbm = 0.0;
                double rsrq_db = 0.0;
                double sinr_db = 0.0;
                std::string cell_id = "N/A";
            } strength;
            
            struct Connection {
                std::string status = "Disconnected";
                std::string network = "N/A";
                std::string technology = "N/A";
                std::string band = "N/A";
                std::string apn = "N/A";
                double data_usage_mb = 0.0;
            } connection;
        } signal;
    };

    SystemDataCollector();
    ~SystemDataCollector();
    
    // Control methods
    bool start(int poll_interval_seconds = 2);
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // Data access
    SystemMetrics getCurrentMetrics() const;
    json getMetricsAsJson() const;
    
    // Configuration
    void setPollInterval(int seconds) { poll_interval_seconds_ = seconds; }
    int getPollInterval() const { return poll_interval_seconds_; }
    void setCollectionProgressLogInterval(int interval) { collection_progress_log_interval_ = interval; }
    int getCollectionProgressLogInterval() const { return collection_progress_log_interval_; }

private:
    std::atomic<bool> running_;
    std::thread collector_thread_;
    int poll_interval_seconds_;
    int collection_progress_log_interval_;
    
    // Current metrics (thread-safe access)
    mutable std::mutex metrics_mutex_;
    SystemMetrics current_metrics_;
    
    // Collection methods
    void collectLoop();
    void collectAllMetrics();
    
    // Individual metric collectors
    void collectCPUMetrics(SystemMetrics::CPU& cpu);
    void collectRAMMetrics(SystemMetrics::RAM& ram);
    void collectSwapMetrics(SystemMetrics::Swap& swap);
    void collectNetworkMetrics(SystemMetrics::Network& network);
    void collectUltimaServerMetrics(SystemMetrics::UltimaServer& server);
    void collectSignalMetrics(SystemMetrics::Signal& signal);
    
    // Utility methods
    double getCPUUsage();
    int getCPUCoreCount();
    double getCPUTemperature();
    double getCPUFrequency();
    double getRAMUsagePercent();
    double getRAMUsedGB();
    double getRAMTotalGB();
    double getSwapUsagePercent();
    double getSwapUsedMB();
    double getSwapTotalGB();
    std::string getExternalIP();
    std::string getLocalIP();
    std::string getGateway();
    std::string getMACAddress();
    double getNetworkLatency();
    std::string getNetworkInterface();
    
    // File reading utilities
    std::string readFile(const std::string& path);
    double parseDoubleFromProc(const std::string& content, const std::string& key);
    long parseLongFromProc(const std::string& content, const std::string& key);
    
    // Error handling
    void logError(const std::string& message) const;
};

#endif // SYSTEM_DATA_COLLECTOR_H
