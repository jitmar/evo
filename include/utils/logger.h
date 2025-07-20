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
    DEBUG = 1,      ///< Debug level
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
    void debug(const std::string& message) { log(LogLevel::DEBUG, message); }

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
    void setLevel(LogLevel level) { config_.level = level; }

    /**
     * @brief Get log level
     * @return Current log level
     */
    LogLevel getLevel() const { return config_.level; }

    /**
     * @brief Set log file
     * @param filename Log file path
     * @return True if successful
     */
    bool setLogFile(const std::string& filename);

    /**
     * @brief Get logger configuration
     * @return Current configuration
     */
    const LoggerConfig& getConfig() const { return config_; }

    /**
     * @brief Set logger configuration
     * @param config New configuration
     */
    void setConfig(const LoggerConfig& config);

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
    static std::string getLevelColor(LogLevel level);

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
    std::mutex mutex_;                    ///< Thread safety mutex
    std::stringstream buffer_;            ///< Log buffer

    /**
     * @brief Initialize logger
     * @return True if initialized successfully
     */
    bool initialize();

    /**
     * @brief Write log message
     * @param level Log level
     * @param message Log message
     */
    void writeLog(LogLevel level, const std::string& message);

    /**
     * @brief Format log message
     * @param level Log level
     * @param message Log message
     * @return Formatted message
     */
    std::string formatMessage(LogLevel level, const std::string& message);

    /**
     * @brief Write to console
     * @param message Message to write
     */
    void writeToConsole(const std::string& message);

    /**
     * @brief Write to file
     * @param message Message to write
     */
    void writeToFile(const std::string& message);

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

/**
 * @brief Log macro for convenience
 */
#define EVOSIM_LOG(level, message) \
    do { \
        if (auto logger = evosim::getLogger()) { \
            logger->log(level, message); \
        } \
    } while(0)

#define EVOSIM_TRACE(message) EVOSIM_LOG(evosim::LogLevel::TRACE, message)
#define EVOSIM_DEBUG(message) EVOSIM_LOG(evosim::LogLevel::DEBUG, message)
#define EVOSIM_INFO(message)  EVOSIM_LOG(evosim::LogLevel::INFO, message)
#define EVOSIM_WARN(message)  EVOSIM_LOG(evosim::LogLevel::WARNING, message)
#define EVOSIM_ERROR(message) EVOSIM_LOG(evosim::LogLevel::ERROR, message)
#define EVOSIM_FATAL(message) EVOSIM_LOG(evosim::LogLevel::FATAL, message)

} // namespace evosim 