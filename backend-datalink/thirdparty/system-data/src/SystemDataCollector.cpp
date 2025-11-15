#include "SystemDataCollector.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <mutex>
#include <algorithm>
#include <cstdio>
#include <memory>

SystemDataCollector::SystemDataCollector() 
    : running_(false), poll_interval_seconds_(2), collection_progress_log_interval_(30) {
    // Initialize metrics with default values
    current_metrics_ = SystemMetrics{};
}

SystemDataCollector::~SystemDataCollector() {
    stop();
}

bool SystemDataCollector::start(int poll_interval_seconds) {
    if (running_.load()) {
        logError("SystemDataCollector is already running");
        return false;
    }
    
    poll_interval_seconds_ = poll_interval_seconds;
    running_.store(true);
    
    collector_thread_ = std::thread(&SystemDataCollector::collectLoop, this);
    
    std::cout << "[SystemDataCollector] Started with " << poll_interval_seconds_ << "s interval" << std::endl;
    return true;
}

void SystemDataCollector::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    
    if (collector_thread_.joinable()) {
        collector_thread_.join();
    }
    
    std::cout << "[SystemDataCollector] Stopped" << std::endl;
}

SystemDataCollector::SystemMetrics SystemDataCollector::getCurrentMetrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return current_metrics_;
}

json SystemDataCollector::getMetricsAsJson() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    return {
        {"cpu", {
            {"usage_percent", current_metrics_.cpu.usage_percent},
            {"cores", current_metrics_.cpu.cores},
            {"temperature_celsius", current_metrics_.cpu.temperature_celsius},
            {"frequency_ghz", current_metrics_.cpu.frequency_ghz}
        }},
        {"ram", {
            {"usage_percent", current_metrics_.ram.usage_percent},
            {"used_gb", current_metrics_.ram.used_gb},
            {"total_gb", current_metrics_.ram.total_gb}
        }},
        {"swap", {
            {"usage_percent", current_metrics_.swap.usage_percent},
            {"used_mb", current_metrics_.swap.used_mb},
            {"total_gb", current_metrics_.swap.total_gb},
            {"status", current_metrics_.swap.status}
        }},
        {"network", {
            {"internet", {
                {"status", current_metrics_.network.internet.status},
                {"external_ip", current_metrics_.network.internet.external_ip},
                {"dns_primary", current_metrics_.network.internet.dns_primary},
                {"dns_secondary", current_metrics_.network.internet.dns_secondary},
                {"latency_ms", current_metrics_.network.internet.latency_ms},
                {"bandwidth", current_metrics_.network.internet.bandwidth}
            }},
            {"connection", {
                {"status", current_metrics_.network.connection.status},
                {"interface", current_metrics_.network.connection.interface_name},
                {"mac_address", current_metrics_.network.connection.mac_address},
                {"local_ip", current_metrics_.network.connection.local_ip},
                {"gateway", current_metrics_.network.connection.gateway},
                {"speed", current_metrics_.network.connection.speed}
            }}
        }},
        {"ultima_server", {
            {"status", current_metrics_.ultima_server.status},
            {"server", current_metrics_.ultima_server.server},
            {"port", current_metrics_.ultima_server.port},
            {"protocol", current_metrics_.ultima_server.protocol},
            {"last_ping_ms", current_metrics_.ultima_server.last_ping_ms},
            {"session", current_metrics_.ultima_server.session}
        }},
        {"signal", {
            {"strength", {
                {"status", current_metrics_.signal.strength.status},
                {"rssi_dbm", current_metrics_.signal.strength.rssi_dbm},
                {"rsrp_dbm", current_metrics_.signal.strength.rsrp_dbm},
                {"rsrq_db", current_metrics_.signal.strength.rsrq_db},
                {"sinr_db", current_metrics_.signal.strength.sinr_db},
                {"cell_id", current_metrics_.signal.strength.cell_id}
            }},
            {"connection", {
                {"status", current_metrics_.signal.connection.status},
                {"network", current_metrics_.signal.connection.network},
                {"technology", current_metrics_.signal.connection.technology},
                {"band", current_metrics_.signal.connection.band},
                {"apn", current_metrics_.signal.connection.apn},
                {"data_usage_mb", current_metrics_.signal.connection.data_usage_mb}
            }}
        }}
    };
}

void SystemDataCollector::collectLoop() {
    int collection_count = 0;
    
    while (running_.load()) {
        try {
            collectAllMetrics();
            collection_count++;
            
            // Log collection progress periodically (configurable interval)
            if (collection_count % collection_progress_log_interval_ == 1) {
                std::cout << "[SystemDataCollector] Collected metrics #" << collection_count 
                         << " (CPU: " << current_metrics_.cpu.usage_percent << "%, "
                         << "RAM: " << current_metrics_.ram.usage_percent << "%)" << std::endl;
            }
            
        } catch (const std::exception& e) {
            logError("Error in collection loop: " + std::string(e.what()));
        }
        
        // Sleep for the configured interval
        std::this_thread::sleep_for(std::chrono::seconds(poll_interval_seconds_));
    }
    
    std::cout << "[SystemDataCollector] Collection loop stopped after " << collection_count << " collections" << std::endl;
}

void SystemDataCollector::collectAllMetrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    collectCPUMetrics(current_metrics_.cpu);
    collectRAMMetrics(current_metrics_.ram);
    collectSwapMetrics(current_metrics_.swap);
    collectNetworkMetrics(current_metrics_.network);
    collectUltimaServerMetrics(current_metrics_.ultima_server);
    collectSignalMetrics(current_metrics_.signal);
}

void SystemDataCollector::collectCPUMetrics(SystemMetrics::CPU& cpu) {
    cpu.usage_percent = getCPUUsage();
    cpu.cores = getCPUCoreCount();
    cpu.temperature_celsius = getCPUTemperature();
    cpu.frequency_ghz = getCPUFrequency();
}

void SystemDataCollector::collectRAMMetrics(SystemMetrics::RAM& ram) {
    ram.usage_percent = getRAMUsagePercent();
    ram.used_gb = getRAMUsedGB();
    ram.total_gb = getRAMTotalGB();
}

void SystemDataCollector::collectSwapMetrics(SystemMetrics::Swap& swap) {
    swap.usage_percent = getSwapUsagePercent();
    swap.used_mb = getSwapUsedMB();
    swap.total_gb = getSwapTotalGB();
    swap.status = swap.usage_percent > 80.0 ? "High" : "Normal";
}

void SystemDataCollector::collectNetworkMetrics(SystemMetrics::Network& network) {
    network.internet.external_ip = getExternalIP();
    network.internet.latency_ms = getNetworkLatency();
    network.connection.local_ip = getLocalIP();
    network.connection.gateway = getGateway();
    network.connection.interface_name = getNetworkInterface();
    network.connection.mac_address = getMACAddress();
    
    // Set status based on connectivity
    network.internet.status = network.internet.external_ip != "N/A" ? "Connected" : "Unknown";
    network.connection.status = network.connection.local_ip != "N/A" ? "Connected" : "Unknown";
}

void SystemDataCollector::collectUltimaServerMetrics(SystemMetrics::UltimaServer& server) {
    // Placeholder implementation - keep default values
    server.status = "Unknown";
    server.server = "N/A";
    server.port = 0;
    server.protocol = "N/A";
    server.last_ping_ms = 0.0;
    server.session = "N/A";
}

void SystemDataCollector::collectSignalMetrics(SystemMetrics::Signal& signal) {
    // Placeholder implementation - cellular modem specific metrics
    // These would require specific modem hardware and APIs
    signal.strength.status = "No Signal";
    signal.strength.rssi_dbm = 0.0;
    signal.strength.rsrp_dbm = 0.0;
    signal.strength.rsrq_db = 0.0;
    signal.strength.sinr_db = 0.0;
    signal.strength.cell_id = "N/A";
    
    signal.connection.status = "Disconnected";
    signal.connection.network = "N/A";
    signal.connection.technology = "N/A";
    signal.connection.band = "N/A";
    signal.connection.apn = "N/A";
    signal.connection.data_usage_mb = 0.0;
}

double SystemDataCollector::getCPUUsage() {
    try {
        std::string stat = readFile("/proc/stat");
        if (stat.empty()) return 0.0;
        
        std::istringstream iss(stat);
        std::string cpu_label;
        long user, nice, system, idle, iowait, irq, softirq, steal;
        
        iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
        
        long total = user + nice + system + idle + iowait + irq + softirq + steal;
        long idle_time = idle + iowait;
        
        static long prev_total = 0, prev_idle = 0;
        
        if (prev_total > 0) {
            long total_diff = total - prev_total;
            long idle_diff = idle_time - prev_idle;
            
            if (total_diff > 0) {
                double usage = 100.0 * (1.0 - (double)idle_diff / total_diff);
                prev_total = total;
                prev_idle = idle_time;
                return std::max(0.0, std::min(100.0, usage));
            }
        }
        
        prev_total = total;
        prev_idle = idle_time;
        return 0.0;
        
    } catch (const std::exception& e) {
        logError("Failed to get CPU usage: " + std::string(e.what()));
        return 0.0;
    }
}

int SystemDataCollector::getCPUCoreCount() {
    try {
        std::string cpuinfo = readFile("/proc/cpuinfo");
        if (cpuinfo.empty()) return 1;
        
        std::istringstream iss(cpuinfo);
        std::string line;
        int core_count = 0;
        
        while (std::getline(iss, line)) {
            if (line.find("processor") == 0) {
                core_count++;
            }
        }
        
        return std::max(1, core_count);
        
    } catch (const std::exception& e) {
        logError("Failed to get CPU core count: " + std::string(e.what()));
        return 1;
    }
}

double SystemDataCollector::getCPUTemperature() {
    try {
        // Try common thermal zones
        std::vector<std::string> thermal_paths = {
            "/sys/class/thermal/thermal_zone0/temp",
            "/sys/class/hwmon/hwmon0/temp1_input",
            "/sys/devices/virtual/thermal/thermal_zone0/temp"
        };
        
        for (const auto& path : thermal_paths) {
            std::string temp_str = readFile(path);
            if (!temp_str.empty()) {
                double temp_millidegrees = std::stod(temp_str);
                return temp_millidegrees / 1000.0; // Convert to Celsius
            }
        }
        
        return 0.0; // Temperature not available
        
    } catch (const std::exception& e) {
        logError("Failed to get CPU temperature: " + std::string(e.what()));
        return 0.0;
    }
}

double SystemDataCollector::getCPUFrequency() {
    try {
        std::string freq_str = readFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (!freq_str.empty()) {
            double freq_khz = std::stod(freq_str);
            return freq_khz / 1000000.0; // Convert to GHz
        }
        
        // Fallback to /proc/cpuinfo
        std::string cpuinfo = readFile("/proc/cpuinfo");
        if (!cpuinfo.empty()) {
            size_t pos = cpuinfo.find("cpu MHz");
            if (pos != std::string::npos) {
                size_t colon = cpuinfo.find(":", pos);
                if (colon != std::string::npos) {
                    std::string mhz_str = cpuinfo.substr(colon + 1);
                    size_t end = mhz_str.find('\n');
                    if (end != std::string::npos) {
                        mhz_str = mhz_str.substr(0, end);
                    }
                    double mhz = std::stod(mhz_str);
                    return mhz / 1000.0; // Convert to GHz
                }
            }
        }
        
        return 0.0;
        
    } catch (const std::exception& e) {
        logError("Failed to get CPU frequency: " + std::string(e.what()));
        return 0.0;
    }
}

double SystemDataCollector::getRAMUsagePercent() {
    try {
        std::string meminfo = readFile("/proc/meminfo");
        if (meminfo.empty()) return 0.0;
        
        long total_kb = parseLongFromProc(meminfo, "MemTotal:");
        long available_kb = parseLongFromProc(meminfo, "MemAvailable:");
        
        if (total_kb > 0) {
            long used_kb = total_kb - available_kb;
            return 100.0 * (double)used_kb / total_kb;
        }
        
        return 0.0;
        
    } catch (const std::exception& e) {
        logError("Failed to get RAM usage: " + std::string(e.what()));
        return 0.0;
    }
}

double SystemDataCollector::getRAMUsedGB() {
    try {
        std::string meminfo = readFile("/proc/meminfo");
        if (meminfo.empty()) return 0.0;
        
        long total_kb = parseLongFromProc(meminfo, "MemTotal:");
        long available_kb = parseLongFromProc(meminfo, "MemAvailable:");
        
        if (total_kb > 0) {
            long used_kb = total_kb - available_kb;
            return used_kb / (1024.0 * 1024.0); // Convert to GB
        }
        
        return 0.0;
        
    } catch (const std::exception& e) {
        logError("Failed to get RAM used: " + std::string(e.what()));
        return 0.0;
    }
}

double SystemDataCollector::getRAMTotalGB() {
    try {
        std::string meminfo = readFile("/proc/meminfo");
        if (meminfo.empty()) return 0.0;
        
        long total_kb = parseLongFromProc(meminfo, "MemTotal:");
        return total_kb / (1024.0 * 1024.0); // Convert to GB
        
    } catch (const std::exception& e) {
        logError("Failed to get RAM total: " + std::string(e.what()));
        return 0.0;
    }
}

double SystemDataCollector::getSwapUsagePercent() {
    try {
        std::string meminfo = readFile("/proc/meminfo");
        if (meminfo.empty()) return 0.0;
        
        long total_kb = parseLongFromProc(meminfo, "SwapTotal:");
        long free_kb = parseLongFromProc(meminfo, "SwapFree:");
        
        if (total_kb > 0) {
            long used_kb = total_kb - free_kb;
            return 100.0 * (double)used_kb / total_kb;
        }
        
        return 0.0;
        
    } catch (const std::exception& e) {
        logError("Failed to get swap usage: " + std::string(e.what()));
        return 0.0;
    }
}

double SystemDataCollector::getSwapUsedMB() {
    try {
        std::string meminfo = readFile("/proc/meminfo");
        if (meminfo.empty()) return 0.0;
        
        long total_kb = parseLongFromProc(meminfo, "SwapTotal:");
        long free_kb = parseLongFromProc(meminfo, "SwapFree:");
        
        if (total_kb > 0) {
            long used_kb = total_kb - free_kb;
            return used_kb / 1024.0; // Convert to MB
        }
        
        return 0.0;
        
    } catch (const std::exception& e) {
        logError("Failed to get swap used: " + std::string(e.what()));
        return 0.0;
    }
}

double SystemDataCollector::getSwapTotalGB() {
    try {
        std::string meminfo = readFile("/proc/meminfo");
        if (meminfo.empty()) return 0.0;
        
        long total_kb = parseLongFromProc(meminfo, "SwapTotal:");
        return total_kb / (1024.0 * 1024.0); // Convert to GB
        
    } catch (const std::exception& e) {
        logError("Failed to get swap total: " + std::string(e.what()));
        return 0.0;
    }
}

std::string SystemDataCollector::getExternalIP() {
    try {
        // Use curl to get external IP (cached result)
        std::string cmd = "curl -s --connect-timeout 5 --max-time 10 ifconfig.me 2>/dev/null";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        
        if (!pipe) return "N/A";
        
        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result += buffer;
        }
        
        // Remove whitespace
        result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
        
        return result.empty() ? "N/A" : result;
        
    } catch (const std::exception& e) {
        logError("Failed to get external IP: " + std::string(e.what()));
        return "N/A";
    }
}

std::string SystemDataCollector::getLocalIP() {
    try {
        std::string cmd = "hostname -I | awk '{print $1}' 2>/dev/null";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        
        if (!pipe) return "N/A";
        
        char buffer[128];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result = buffer;
            result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
        }
        
        return result.empty() ? "N/A" : result;
        
    } catch (const std::exception& e) {
        logError("Failed to get local IP: " + std::string(e.what()));
        return "N/A";
    }
}

std::string SystemDataCollector::getGateway() {
    try {
        std::string cmd = "ip route | grep default | awk '{print $3}' 2>/dev/null";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        
        if (!pipe) return "N/A";
        
        char buffer[128];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result = buffer;
            result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
        }
        
        return result.empty() ? "N/A" : result;
        
    } catch (const std::exception& e) {
        logError("Failed to get gateway: " + std::string(e.what()));
        return "N/A";
    }
}

std::string SystemDataCollector::getMACAddress() {
    try {
        std::string cmd = "ip link show | grep -E 'link/ether' | head -1 | awk '{print $2}' 2>/dev/null";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        
        if (!pipe) return "N/A";
        
        char buffer[128];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result = buffer;
            result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
        }
        
        return result.empty() ? "N/A" : result;
        
    } catch (const std::exception& e) {
        logError("Failed to get MAC address: " + std::string(e.what()));
        return "N/A";
    }
}

double SystemDataCollector::getNetworkLatency() {
    try {
        std::string cmd = "ping -c 1 8.8.8.8 2>/dev/null | grep 'time=' | awk -F'time=' '{print $2}' | awk '{print $1}'";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        
        if (!pipe) return 0.0;
        
        char buffer[128];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result = buffer;
            result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
        }
        
        return result.empty() ? 0.0 : std::stod(result);
        
    } catch (const std::exception& e) {
        logError("Failed to get network latency: " + std::string(e.what()));
        return 0.0;
    }
}

std::string SystemDataCollector::getNetworkInterface() {
    try {
        std::string cmd = "ip route | grep default | awk '{print $5}' 2>/dev/null";
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        
        if (!pipe) return "N/A";
        
        char buffer[128];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result = buffer;
            result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
        }
        
        return result.empty() ? "N/A" : result;
        
    } catch (const std::exception& e) {
        logError("Failed to get network interface: " + std::string(e.what()));
        return "N/A";
    }
}

std::string SystemDataCollector::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

double SystemDataCollector::parseDoubleFromProc(const std::string& content, const std::string& key) {
    size_t pos = content.find(key);
    if (pos == std::string::npos) return 0.0;
    
    size_t start = pos + key.length();
    while (start < content.length() && isspace(content[start])) {
        start++;
    }
    
    size_t end = start;
    while (end < content.length() && !isspace(content[end])) {
        end++;
    }
    
    std::string value = content.substr(start, end - start);
    try {
        return std::stod(value);
    } catch (...) {
        return 0.0;
    }
}

long SystemDataCollector::parseLongFromProc(const std::string& content, const std::string& key) {
    size_t pos = content.find(key);
    if (pos == std::string::npos) return 0;
    
    size_t start = pos + key.length();
    while (start < content.length() && isspace(content[start])) {
        start++;
    }
    
    size_t end = start;
    while (end < content.length() && !isspace(content[end])) {
        end++;
    }
    
    std::string value = content.substr(start, end - start);
    try {
        return std::stol(value);
    } catch (...) {
        return 0;
    }
}

void SystemDataCollector::logError(const std::string& message) const {
    std::cerr << "[SystemDataCollector] ERROR: " << message << std::endl;
}
