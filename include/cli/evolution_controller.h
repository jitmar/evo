#pragma once

#include "core/evolution_engine.h"
#include "command_processor.h"
#include <memory>
#include <string>
#include <thread>
#include <atomic>

namespace evosim {

/**
 * @brief High-level controller for evolution simulation
 * 
 * Provides a unified interface for controlling the evolution process,
 * managing the CLI, and handling user interactions.
 */
class EvolutionController {
public:
    using EnginePtr = std::shared_ptr<EvolutionEngine>;
    using ProcessorPtr = std::shared_ptr<CommandProcessor>;

    /**
     * @brief Controller configuration
     */
    struct Config {
        bool enable_cli;              ///< Enable CLI interface
        bool enable_interactive;      ///< Enable interactive mode
        bool enable_auto_save;        ///< Enable automatic saving
        std::string config_file;      ///< Configuration file
        std::string log_file;         ///< Log file
        bool enable_colors;           ///< Enable colored output
        bool enable_progress;         ///< Enable progress indicators
        uint32_t max_history;         ///< Maximum command history
        bool enable_completion;       ///< Enable command completion
        
        Config() : enable_cli(true), enable_interactive(true), enable_auto_save(true),
                   config_file("evosim.conf"), log_file("evosim.log"), enable_colors(true),
                   enable_progress(true), max_history(1000), enable_completion(true) {}
    };

    /**
     * @brief Controller state
     */
    enum class State {
        INITIALIZED,        ///< Controller initialized
        RUNNING,           ///< Evolution running
        PAUSED,            ///< Evolution paused
        STOPPED,           ///< Evolution stopped
        ERROR              ///< Error state
    };

    /**
     * @brief Constructor
     * @param config Controller configuration
     */
    explicit EvolutionController(const Config& config = Config());

    /**
     * @brief Destructor
     */
    ~EvolutionController();

    /**
     * @brief Initialize controller
     * @return True if initialized successfully
     */
    bool initialize();

    /**
     * @brief Run controller in interactive mode
     * @return Exit code
     */
    int runInteractive();

    /**
     * @brief Run controller with command line arguments
     * @param argc Argument count
     * @param argv Argument vector
     * @return Exit code
     */
    int run(int argc, char* argv[]);

    /**
     * @brief Process single command
     * @param command Command to process
     * @return True if successful
     */
    bool processCommand(const std::string& command);

    /**
     * @brief Start evolution
     * @return True if started successfully
     */
    bool startEvolution();

    /**
     * @brief Stop evolution
     * @return True if stopped successfully
     */
    bool stopEvolution();

    /**
     * @brief Pause evolution
     * @return True if paused successfully
     */
    bool pauseEvolution();

    /**
     * @brief Resume evolution
     * @return True if resumed successfully
     */
    bool resumeEvolution();

    /**
     * @brief Get controller state
     * @return Current state
     */
    State getState() const { return state_; }

    /**
     * @brief Get evolution engine
     * @return Engine pointer
     */
    EnginePtr getEngine() const { return engine_; }

    /**
     * @brief Get command processor
     * @return Processor pointer
     */
    ProcessorPtr getProcessor() const { return processor_; }

    /**
     * @brief Get controller configuration
     * @return Current configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * @brief Set controller configuration
     * @param config New configuration
     */
    void setConfig(const Config& config) { config_ = config; }

    /**
     * @brief Load configuration from file
     * @param filename Configuration file
     * @return True if loaded successfully
     */
    bool loadConfig(const std::string& filename);

    /**
     * @brief Save configuration to file
     * @param filename Configuration file
     * @return True if saved successfully
     */
    bool saveConfig(const std::string& filename);

    /**
     * @brief Get controller statistics
     * @return Statistics string
     */
    std::string getStatistics() const;

    /**
     * @brief Shutdown controller
     * @return True if shutdown successful
     */
    bool shutdown();

    /**
     * @brief Check if controller is running
     * @return True if running
     */
    bool isRunning() const { return state_ == State::RUNNING; }

    /**
     * @brief Check if controller is paused
     * @return True if paused
     */
    bool isPaused() const { return state_ == State::PAUSED; }

    /**
     * @brief Wait for evolution to complete
     * @param timeout_ms Timeout in milliseconds (0 for infinite)
     * @return True if completed within timeout
     */
    bool waitForCompletion(uint32_t timeout_ms = 0);

    /**
     * @brief Get command history
     * @return Vector of command history
     */
    std::vector<std::string> getCommandHistory() const;

    /**
     * @brief Clear command history
     */
    void clearCommandHistory();

    /**
     * @brief Export evolution data
     * @param filename Output filename
     * @return True if successful
     */
    bool exportData(const std::string& filename);

private:
    Config config_;                     ///< Controller configuration
    State state_;                       ///< Controller state
    EnginePtr engine_;                  ///< Evolution engine
    ProcessorPtr processor_;            ///< Command processor
    std::thread cli_thread_;            ///< CLI thread
    std::atomic<bool> should_exit_;     ///< Exit flag
    std::vector<std::string> history_;  ///< Command history
    mutable std::mutex mutex_;          ///< Thread safety mutex

    /**
     * @brief Initialize evolution engine
     * @return True if initialized successfully
     */
    bool initializeEngine();

    /**
     * @brief Initialize command processor
     * @return True if initialized successfully
     */
    bool initializeProcessor();

    /**
     * @brief Run CLI loop
     */
    void runCLILoop();

    /**
     * @brief Handle evolution events
     * @param event Evolution event
     */
    void handleEvolutionEvent(const EvolutionEngine::Event& event);

    /**
     * @brief Update controller state
     * @param new_state New state
     */
    void updateState(State new_state);

    /**
     * @brief Load default configuration
     */
    void loadDefaultConfig();

    /**
     * @brief Setup signal handlers
     */
    void setupSignalHandlers();

    /**
     * @brief Cleanup resources
     */
    void cleanup();

    /**
     * @brief Print welcome message
     */
    void printWelcome() const;

    /**
     * @brief Print prompt
     */
    void printPrompt() const;

    /**
     * @brief Read command line
     * @return Command line string
     */
    std::string readCommandLine() const;

    /**
     * @brief Add command to history
     * @param command Command to add
     */
    void addToHistory(const std::string& command);

    /**
     * @brief Get command completion suggestions
     * @param partial_command Partial command
     * @return Vector of suggestions
     */
    std::vector<std::string> getCompletions(const std::string& partial_command) const;
};

} // namespace evosim 