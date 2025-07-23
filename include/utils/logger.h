#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <fstream>
#include <sstream>
#include <chrono>

namespace evosim {

/**
 * @brief Logging levels
 */
enum class LogLevel {
    TRACE = 0,      ///< Trace level (most verbose)
    DBG = 1,      ///< Debug level
    INFO = 2,       ///< Info level
    WARNING = 3,    ///< Warning level
    ERROR = 4,      ///< Error level
    FATAL = 5       ///< Fatal level (least verbose)
};

/**
 * @brief Logger configuration
 */
struct LoggerConfig {
    LogLevel level = LogLevel::INFO;        ///< Minimum log level
    std::string filename = "";              ///< Log file (empty for console only)
    bool enable_console = true;             ///< Enable console output
    bool enable_file = false;               ///< Enable file output
    bool enable_timestamp = true;           ///< Enable timestamps
    bool enable_thread_id = false;          ///< Enable thread IDs
    bool enable_colors = true;              ///< Enable colored output
    uint32_t max_file_size_mb = 100;        ///< Maximum log file size
    uint32_t max_files = 5;                 ///< Maximum number of log files
    bool auto_flush = true;                 ///< Auto-flush after each log
    std::string format = "[{level}] {timestamp} {message}"; ///< Log format
};

/**
 * @brief Logger class for comprehensive logging
 * 
 * Provides thread-safe logging with multiple output destinations,
 * log rotation, and configurable formatting.
 */
class Logger {
public:
    /**
     * @brief Constructor
     * @param config Logger configuration
     */
    explicit Logger(const LoggerConfig& config = LoggerConfig{});

    /**
     * @brief Destructor
     */
    ~Logger();

    /**
     * @brief Log message at specified level
     * @param level Log level
     * @param message Log message
     */
    void log(LogLevel level, const std::string& message);

    /**
     * @brief Log trace message
     * @param message Log message
     */
    void trace(const std::string& message) { log(LogLevel::TRACE, message); }

    /**
     * @brief Log debug message
     * @param message Log message
     */
    void debug(const std::string& message) { log(LogLevel::DBG, message); }

    /**
     * @brief Log info message
     * @param message Log message
     */
    void info(const std::string& message) { log(LogLevel::INFO, message); }

    /**
     * @brief Log warning message
     * @param message Log message
     */
    void warning(const std::string& message) { log(LogLevel::WARNING, message); }

    /**
     * @brief Log error message
     * @param message Log message
     */
    void error(const std::string& message) { log(LogLevel::ERROR, message); }

    /**
     * @brief Log fatal message
     * @param message Log message
     */
    void fatal(const std::string& message) { log(LogLevel::FATAL, message); }

    /**
     * @brief Set log level
     * @param level New log level
     */
    void setLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.level = level;
    }

    /**
     * @brief Get log level
     * @return Current log level
     */
    LogLevel getLevel() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_.level;
    }

    /**
     * @brief Set log file
     * @param filename Log file path
     * @return True if successful
     */
    bool setLogFile(const std::string& filename); // Note: Add lock in .cpp implementation

    /**
     * @brief Get logger configuration
     * @return Current configuration
     */
    LoggerConfig getConfig() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    /**
     * @brief Set logger configuration
     * @param config New configuration
     */
    void setConfig(const LoggerConfig& config); // Note: Add lock in .cpp implementation

    /**
     * @brief Flush log buffers
     */
    void flush();

    /**
     * @brief Rotate log files
     * @return True if successful
     */
    bool rotate();

    /**
     * @brief Get log level string
     * @param level Log level
     * @return Level string
     */
    static std::string getLevelString(LogLevel level);

    /**
     * @brief Get log level color
     * @param level Log level
     * @return ANSI color code
     */
    std::string getLevelColor(LogLevel level);

    /**
     * @brief Parse log level from string
     * @param level_str Level string
     * @return Log level
     */
    static LogLevel parseLevel(const std::string& level_str);

    /**
     * @brief Get current timestamp string
     * @return Timestamp string
     */
    static std::string getTimestamp();

    /**
     * @brief Get current thread ID
     * @return Thread ID string
     */
    static std::string getThreadId();

private:
    LoggerConfig config_;                 ///< Logger configuration
    std::ofstream file_stream_;           ///< Log file stream
    mutable std::mutex mutex_;            ///< Thread safety mutex
    std::stringstream buffer_;            ///< Log buffer

    /**
     * @brief Initialize logger
     * @return True if initialized successfully
     */
    bool initialize();

    /**
     * @brief Format log message
     * @param level Log level
     * @param message Log message
     * @return Formatted message
     */
    std::string formatMessage(LogLevel level, const std::string& message);

    // Internal unlocked helpers for thread safety
    void writeLog_unlocked(LogLevel level, const std::string& message);
    void writeToConsole_unlocked(LogLevel level, const std::string& message);
    void writeToFile_unlocked(const std::string& message);
    void flush_unlocked();

    /**
     * @brief Check if log file needs rotation
     * @return True if rotation needed
     */
    bool needsRotation() const;

    /**
     * @brief Perform log rotation
     * @return True if successful
     */
    bool performRotation();

    /**
     * @brief Close log file
     */
    void closeFile();

    /**
     * @brief Open log file
     * @return True if successful
     */
    bool openFile();
};

/**
 * @brief Global logger instance
 */
extern std::unique_ptr<Logger> g_logger;

/**
 * @brief Initialize global logger
 * @param config Logger configuration
 */
void initializeLogger(const LoggerConfig& config = LoggerConfig{});

/**
 * @brief Get global logger
 * @return Logger pointer
 */
Logger* getLogger();

/**
 * @brief Shutdown global logger
 */
void shutdownLogger();

// --- Inline Logging Functions (Macro-Free Alternative) ---

inline void log_trace(const std::string& message) {
    if (auto logger = getLogger()) {
        logger->trace(message);
    }
}

inline void log_debug(const std::string& message) {
    if (auto logger = getLogger()) {
        logger->debug(message);
    }
}

inline void log_info(const std::string& message) {
    if (auto logger = getLogger()) {
        logger->info(message);
    }
}

inline void log_warn(const std::string& message) {
    if (auto logger = getLogger()) {
        logger->warning(message);
    }
}

inline void log_error(const std::string& message) {
    if (auto logger = getLogger()) {
        logger->error(message);
    }
}

inline void log_fatal(const std::string& message) {
    if (auto logger = getLogger()) {
        logger->fatal(message);
    }
}

} // namespace evosim 