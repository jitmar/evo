#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

namespace evosim {

class EvolutionEngine;

/**
 * @brief Command processor for CLI interface
 * 
 * Handles parsing and execution of user commands for controlling
 * the evolution simulation.
 */
class CommandProcessor {
public:
    using EnginePtr = std::shared_ptr<EvolutionEngine>;
    using CommandFunction = std::function<bool(const std::vector<std::string>&)>;

    /**
     * @brief Command result
     */
    struct CommandResult {
        bool success;                    ///< Command success status
        std::string message;             ///< Result message
        std::string error;               ///< Error message (if any)
        bool should_exit;                ///< Whether to exit CLI
    };

    /**
     * @brief Command information
     */
    struct CommandInfo {
        std::string name;                ///< Command name
        std::string description;         ///< Command description
        std::string usage;               ///< Usage syntax
        std::vector<std::string> aliases; ///< Command aliases
        bool requires_engine;            ///< Whether engine is required
    };

    /**
     * @brief Constructor
     * @param engine Evolution engine to control
     */
    explicit CommandProcessor(EnginePtr engine = nullptr);

    /**
     * @brief Destructor
     */
    ~CommandProcessor() = default;

    /**
     * @brief Process command string
     * @param command_line Command line to process
     * @return Command result
     */
    CommandResult processCommand(const std::string& command_line);

    /**
     * @brief Process command arguments
     * @param args Command arguments
     * @return Command result
     */
    CommandResult processCommand(const std::vector<std::string>& args);

    /**
     * @brief Set evolution engine
     * @param engine Engine to control
     */
    void setEngine(EnginePtr engine) { engine_ = engine; }

    /**
     * @brief Get evolution engine
     * @return Engine pointer
     */
    EnginePtr getEngine() const { return engine_; }

    /**
     * @brief Get available commands
     * @return Map of command names to command info
     */
    std::map<std::string, CommandInfo> getCommands() const;

    /**
     * @brief Get command help
     * @param command_name Command name
     * @return Help text for command
     */
    std::string getCommandHelp(const std::string& command_name) const;

    /**
     * @brief Get general help
     * @return General help text
     */
    std::string getHelp() const;

    /**
     * @brief Register custom command
     * @param info Command information
     * @param func Command function
     * @return True if registered successfully
     */
    bool registerCommand(const CommandInfo& info, CommandFunction func);

    /**
     * @brief Unregister command
     * @param command_name Command name
     * @return True if unregistered successfully
     */
    bool unregisterCommand(const std::string& command_name);

    /**
     * @brief Parse command line into arguments
     * @param command_line Command line string
     * @return Vector of arguments
     */
    static std::vector<std::string> parseCommandLine(const std::string& command_line);

    /**
     * @brief Escape string for command line
     * @param str String to escape
     * @return Escaped string
     */
    static std::string escapeString(const std::string& str);

private:
    EnginePtr engine_;                   ///< Evolution engine
    std::map<std::string, CommandFunction> commands_; ///< Registered commands
    std::map<std::string, CommandInfo> command_info_; ///< Command information
    std::map<std::string, std::string> aliases_;      ///< Command aliases

    /**
     * @brief Initialize built-in commands
     */
    void initializeCommands();

    /**
     * @brief Built-in command implementations
     */
    bool cmdHelp(const std::vector<std::string>& args);
    bool cmdStart(const std::vector<std::string>& args);
    bool cmdStop(const std::vector<std::string>& args);
    bool cmdPause(const std::vector<std::string>& args);
    bool cmdResume(const std::vector<std::string>& args);
    bool cmdStatus(const std::vector<std::string>& args);
    bool cmdStats(const std::vector<std::string>& args);
    bool cmdConfig(const std::vector<std::string>& args);
    bool cmdSave(const std::vector<std::string>& args);
    bool cmdLoad(const std::vector<std::string>& args);
    bool cmdExport(const std::vector<std::string>& args);
    bool cmdOrganism(const std::vector<std::string>& args);
    bool cmdPopulation(const std::vector<std::string>& args);
    bool cmdGeneration(const std::vector<std::string>& args);
    bool cmdClear(const std::vector<std::string>& args);
    bool cmdExit(const std::vector<std::string>& args);
    bool cmdQuit(const std::vector<std::string>& args);

    /**
     * @brief Validate command arguments
     * @param args Command arguments
     * @param min_args Minimum required arguments
     * @param max_args Maximum allowed arguments
     * @return True if valid
     */
    bool validateArgs(const std::vector<std::string>& args, 
                     size_t min_args, size_t max_args = SIZE_MAX) const;

    /**
     * @brief Print formatted output
     * @param format Format string
     * @param ... Format arguments
     */
    void printf(const char* format, ...) const;

    /**
     * @brief Print error message
     * @param message Error message
     */
    void printError(const std::string& message) const;

    /**
     * @brief Print success message
     * @param message Success message
     */
    void printSuccess(const std::string& message) const;
};

} // namespace evosim 