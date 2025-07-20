#include "utils/logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

namespace evosim {

// Global logger instance
std::unique_ptr<Logger> g_logger;

Logger::Logger(const LoggerConfig& config)
    : config_(config) {
    initialize();
}

Logger::~Logger() {
    flush();
    closeFile();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < config_.level) {
        return;
    }
    
    writeLog(level, message);
}

bool Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    closeFile();
    config_.filename = filename;
    config_.enable_file = !filename.empty();
    
    return openFile();
}

void Logger::setConfig(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    initialize();
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (file_stream_.is_open()) {
        file_stream_.flush();
    }
    
    if (config_.auto_flush) {
        std::cout.flush();
    }
}

bool Logger::rotate() {
    std::lock_guard<std::mutex> lock(mutex_);
    return performRotation();
}

std::string Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE:   return "TRACE";
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:                return "UNKNOWN";
    }
}

std::string Logger::getLevelColor(LogLevel level) {
    // Use a default value for enable_colors, or make this non-static if needed
    // Here, we assume colors are enabled by default for static context
    bool enable_colors = true;
    // If you want to use config_, make this a non-static method
    if (!enable_colors) {
        return "";
    }
    switch (level) {
        case LogLevel::TRACE:   return "\033[36m"; // Cyan
        case LogLevel::DEBUG:   return "\033[34m"; // Blue
        case LogLevel::INFO:    return "\033[32m"; // Green
        case LogLevel::WARNING: return "\033[33m"; // Yellow
        case LogLevel::ERROR:   return "\033[31m"; // Red
        case LogLevel::FATAL:   return "\033[35m"; // Magenta
        default:                return "\033[0m";  // Reset
    }
}

LogLevel Logger::parseLevel(const std::string& level_str) {
    std::string upper = level_str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    
    if (upper == "TRACE") return LogLevel::TRACE;
    if (upper == "DEBUG") return LogLevel::DEBUG;
    if (upper == "INFO") return LogLevel::INFO;
    if (upper == "WARNING" || upper == "WARN") return LogLevel::WARNING;
    if (upper == "ERROR") return LogLevel::ERROR;
    if (upper == "FATAL") return LogLevel::FATAL;
    
    return LogLevel::INFO; // Default
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

std::string Logger::getThreadId() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

bool Logger::initialize() {
    if (config_.enable_file && !config_.filename.empty()) {
        return openFile();
    }
    return true;
}

void Logger::writeLog(LogLevel level, const std::string& message) {
    std::string formatted = formatMessage(level, message);
    
    if (config_.enable_console) {
        writeToConsole(formatted);
    }
    
    if (config_.enable_file) {
        writeToFile(formatted);
    }
    
    if (config_.auto_flush) {
        flush();
    }
}

std::string Logger::formatMessage(LogLevel level, const std::string& message) {
    std::string result = config_.format;
    
    // Replace placeholders
    std::string level_str = getLevelString(level);
    std::string timestamp = config_.enable_timestamp ? getTimestamp() : "";
    std::string thread_id = config_.enable_thread_id ? getThreadId() : "";
    
    // Simple placeholder replacement
    size_t pos = 0;
    while ((pos = result.find("{level}", pos)) != std::string::npos) {
        result.replace(pos, 7, level_str);
        pos += level_str.length();
    }
    
    pos = 0;
    while ((pos = result.find("{timestamp}", pos)) != std::string::npos) {
        result.replace(pos, 11, timestamp);
        pos += timestamp.length();
    }
    
    pos = 0;
    while ((pos = result.find("{thread_id}", pos)) != std::string::npos) {
        result.replace(pos, 11, thread_id);
        pos += thread_id.length();
    }
    
    pos = 0;
    while ((pos = result.find("{message}", pos)) != std::string::npos) {
        result.replace(pos, 9, message);
        pos += message.length();
    }
    
    return result;
}

void Logger::writeToConsole(const std::string& message) {
    std::cout << message << std::endl;
}

void Logger::writeToFile(const std::string& message) {
    if (file_stream_.is_open()) {
        file_stream_ << message << std::endl;
    }
}

bool Logger::needsRotation() const {
    if (!config_.enable_file || config_.filename.empty()) {
        return false;
    }
    
    std::ifstream file(config_.filename, std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    
    auto size = file.tellg();
    file.close();
    
    return size > static_cast<std::streamsize>(config_.max_file_size_mb * 1024 * 1024);
}

bool Logger::performRotation() {
    if (!config_.enable_file || config_.filename.empty()) {
        return false;
    }
    
    closeFile();
    
    // Simple rotation: append timestamp
    std::string timestamp = getTimestamp();
    std::replace(timestamp.begin(), timestamp.end(), ':', '-');
    std::replace(timestamp.begin(), timestamp.end(), ' ', '_');
    
    std::string rotated_name = config_.filename + "." + timestamp;
    
    if (std::rename(config_.filename.c_str(), rotated_name.c_str()) != 0) {
        return false;
    }
    
    return openFile();
}

void Logger::closeFile() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

bool Logger::openFile() {
    if (!config_.enable_file || config_.filename.empty()) {
        return true;
    }
    
    file_stream_.open(config_.filename, std::ios::app);
    return file_stream_.is_open();
}

// Global logger functions
void initializeLogger(const LoggerConfig& config) {
    g_logger = std::make_unique<Logger>(config);
}

Logger* getLogger() {
    return g_logger.get();
}

void shutdownLogger() {
    g_logger.reset();
}

} // namespace evosim 