#include "cli/command_processor.h"
#include "core/evolution_engine.h"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cstdarg>

namespace evosim {

CommandProcessor::CommandProcessor(EnginePtr engine)
    : engine_(engine) {
    initializeCommands();
}





CommandProcessor::CommandResult CommandProcessor::processCommand(const std::string& input) {
    if (input.empty()) {
        return {true, "", "", false};
    }
    
    auto args = parseCommandLine(input);
    return processCommand(args);
}

CommandProcessor::CommandResult CommandProcessor::processCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
        return {true, "", "", false};
    }
    
    std::string command = args[0];
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);
    
    // Check aliases
    auto alias_it = aliases_.find(command);
    if (alias_it != aliases_.end()) {
        command = alias_it->second;
    }
    
    // Extract command arguments (skip command name)
    std::vector<std::string> cmd_args(args.begin() + 1, args.end());
    
    // Find and execute command
    auto it = commands_.find(command);
    if (it != commands_.end()) {
        bool success = it->second(cmd_args);
        return {success, success ? "Command executed successfully" : "Command failed", "", false};
    } else {
        return {false, "", "Unknown command: " + command, false};
    }
}

std::vector<std::string> CommandProcessor::parseCommandLine(const std::string& command_line) {
    std::vector<std::string> args;
    std::istringstream iss(command_line);
    std::string arg;
    
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}

std::string CommandProcessor::escapeString(const std::string& str) {
    std::string escaped;
    for (char c : str) {
        if (c == '\\' || c == '"' || c == '\'') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

void CommandProcessor::initializeCommands() {
    // Register built-in commands
    commands_["help"] = [this](const std::vector<std::string>& args) { return cmdHelp(args); };
    commands_["start"] = [this](const std::vector<std::string>& args) { return cmdStart(args); };
    commands_["stop"] = [this](const std::vector<std::string>& args) { return cmdStop(args); };
    commands_["pause"] = [this](const std::vector<std::string>& args) { return cmdPause(args); };
    commands_["resume"] = [this](const std::vector<std::string>& args) { return cmdResume(args); };
    commands_["status"] = [this](const std::vector<std::string>& args) { return cmdStatus(args); };
    commands_["stats"] = [this](const std::vector<std::string>& args) { return cmdStats(args); };
    commands_["config"] = [this](const std::vector<std::string>& args) { return cmdConfig(args); };
    commands_["save"] = [this](const std::vector<std::string>& args) { return cmdSave(args); };
    commands_["load"] = [this](const std::vector<std::string>& args) { return cmdLoad(args); };
    commands_["export"] = [this](const std::vector<std::string>& args) { return cmdExport(args); };
    commands_["organism"] = [this](const std::vector<std::string>& args) { return cmdOrganism(args); };
    commands_["population"] = [this](const std::vector<std::string>& args) { return cmdPopulation(args); };
    commands_["generation"] = [this](const std::vector<std::string>& args) { return cmdGeneration(args); };
    commands_["clear"] = [this](const std::vector<std::string>& args) { return cmdClear(args); };
    commands_["exit"] = [this](const std::vector<std::string>& args) { return cmdExit(args); };
    commands_["quit"] = [this](const std::vector<std::string>& args) { return cmdQuit(args); };
    
    // Set up aliases
    aliases_["h"] = "help";
    aliases_["s"] = "start";
    aliases_["st"] = "stop";
    aliases_["p"] = "pause";
    aliases_["r"] = "resume";
    aliases_["stat"] = "status";
    aliases_["c"] = "config";
    aliases_["q"] = "quit";
}

bool CommandProcessor::cmdHelp(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    std::cout << "Available commands:\n";
    std::cout << "  help                    - Show this help message\n";
    std::cout << "  start                   - Start evolution simulation\n";
    std::cout << "  stop                    - Stop evolution simulation\n";
    std::cout << "  pause                   - Pause evolution simulation\n";
    std::cout << "  resume                  - Resume evolution simulation\n";
    std::cout << "  status                  - Show simulation status\n";
    std::cout << "  stats                   - Show detailed statistics\n";
    std::cout << "  config [key] [value]    - Get/set configuration\n";
    std::cout << "  save <filename>         - Save simulation state\n";
    std::cout << "  load <filename>         - Load simulation state\n";
    std::cout << "  export <filename>       - Export data\n";
    std::cout << "  organism <id>           - Show organism details\n";
    std::cout << "  population              - Show population info\n";
    std::cout << "  generation              - Show generation info\n";
    std::cout << "  clear                   - Clear screen\n";
    std::cout << "  quit/exit               - Exit the CLI\n";
    return true;
}

bool CommandProcessor::cmdStart(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    try {
        bool success = engine_->start();
        if (success) {
            std::cout << "Evolution simulation started successfully.\n";
        } else {
            std::cout << "Failed to start evolution simulation.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Error starting evolution: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdStop(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    try {
        bool success = engine_->stop();
        if (success) {
            std::cout << "Evolution simulation stopped.\n";
        } else {
            std::cout << "Failed to stop evolution simulation.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Error stopping evolution: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdPause(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    try {
        bool success = engine_->pause();
        if (success) {
            std::cout << "Evolution simulation paused.\n";
        } else {
            std::cout << "Failed to pause evolution simulation.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Error pausing evolution: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdResume(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    try {
        bool success = engine_->resume();
        if (success) {
            std::cout << "Evolution simulation resumed.\n";
        } else {
            std::cout << "Failed to resume evolution simulation.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Error resuming evolution: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdStatus(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    try {
        auto stats = engine_->getStats();
        std::cout << "Simulation Status:\n";
        std::cout << "  Running: " << (stats.is_running ? "Yes" : "No") << "\n";
        std::cout << "  Paused: " << (stats.is_paused ? "Yes" : "No") << "\n";
        std::cout << "  Generations: " << stats.total_generations << "\n";
        std::cout << "  Population: " << stats.current_population << "\n";
        std::cout << "  Best Fitness: " << stats.current_best_fitness << "\n";
        std::cout << "  Runtime: " << stats.total_runtime_ms << " ms\n";
    } catch (const std::exception& e) {
        std::cout << "Error getting status: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdStats(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    try {
        auto stats = engine_->getStats();
        std::cout << "Detailed Statistics:\n";
        std::cout << "  Total Generations: " << stats.total_generations << "\n";
        std::cout << "  Total Runtime: " << stats.total_runtime_ms << " ms\n";
        std::cout << "  Generations/Second: " << stats.generations_per_second << "\n";
        std::cout << "  Current Population: " << stats.current_population << "\n";
        std::cout << "  Best Fitness: " << stats.current_best_fitness << "\n";
        std::cout << "  Average Fitness: " << stats.current_avg_fitness << "\n";
    } catch (const std::exception& e) {
        std::cout << "Error getting stats: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdConfig(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    try {
        auto config = engine_->getConfig();
        std::cout << "Current Configuration:\n";
        std::cout << "  Auto Start: " << (config.auto_start ? "Yes" : "No") << "\n";
        std::cout << "  Enable Logging: " << (config.enable_logging ? "Yes" : "No") << "\n";
        std::cout << "  Enable Save State: " << (config.enable_save_state ? "Yes" : "No") << "\n";
        std::cout << "  Enable Visualization: " << (config.enable_visualization ? "Yes" : "No") << "\n";
        std::cout << "  Enable Metrics: " << (config.enable_metrics ? "Yes" : "No") << "\n";
        std::cout << "  Save Interval: " << config.save_interval_generations << " generations\n";
        std::cout << "  Visualization Interval: " << config.visualization_interval << "\n";
        std::cout << "  Metrics Interval: " << config.metrics_interval << "\n";
    } catch (const std::exception& e) {
        std::cout << "Error getting config: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdSave(const std::vector<std::string>& args) {
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    std::string filename = args.empty() ? "" : args[0];
    
    try {
        bool success = engine_->saveState(filename);
        if (success) {
            std::cout << "Simulation state saved successfully.\n";
        } else {
            std::cout << "Failed to save simulation state.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Error saving state: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdLoad(const std::vector<std::string>& args) {
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    if (args.empty()) {
        std::cout << "Error: Please specify a filename.\n";
        return false;
    }
    
    try {
        bool success = engine_->loadState(args[0]);
        if (success) {
            std::cout << "Simulation state loaded successfully.\n";
        } else {
            std::cout << "Failed to load simulation state.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Error loading state: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdExport(const std::vector<std::string>& args) {
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    std::string filename = args.empty() ? "export.csv" : args[0];
    
    try {
        bool success = engine_->exportData(filename);
        if (success) {
            std::cout << "Data exported successfully to " << filename << "\n";
        } else {
            std::cout << "Failed to export data.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Error exporting data: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdOrganism(const std::vector<std::string>& args) {
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    if (args.empty()) {
        std::cout << "Error: Please specify an organism ID.\n";
        return false;
    }
    
    try {
        // This would need to be implemented in the engine
        std::cout << "Organism details for ID " << args[0] << ":\n";
        std::cout << "  (Organism details not implemented)\n";
    } catch (const std::exception& e) {
        std::cout << "Error getting organism details: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdPopulation(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    try {
        auto environment = engine_->getEnvironment();
        if (environment) {
            auto population = environment->getPopulation();
            std::cout << "Population Information:\n";
            std::cout << "  Size: " << population.size() << "\n";
            std::cout << "  (Additional population details not implemented)\n";
        } else {
            std::cout << "No environment available.\n";
        }
    } catch (const std::exception& e) {
        std::cout << "Error getting population info: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdGeneration(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    if (!engine_) {
        std::cout << "Error: No evolution engine available.\n";
        return false;
    }
    
    try {
        auto stats = engine_->getStats();
        std::cout << "Generation Information:\n";
        std::cout << "  Current Generation: " << stats.total_generations << "\n";
        std::cout << "  Last Generation Time: " << stats.last_generation_time.time_since_epoch().count() << "\n";
        std::cout << "  (Additional generation details not implemented)\n";
    } catch (const std::exception& e) {
        std::cout << "Error getting generation info: " << e.what() << "\n";
    }
    
    return true;
}

bool CommandProcessor::cmdClear(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    // Simple screen clear
    std::cout << "\033[2J\033[H"; // ANSI escape sequence to clear screen
    return true;
}

bool CommandProcessor::cmdExit(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    std::cout << "Exiting EvoSim CLI.\n";
    return true;
}

bool CommandProcessor::cmdQuit(const std::vector<std::string>& args) {
    (void)args; // Suppress unused parameter warning
    std::cout << "Exiting EvoSim CLI.\n";
    return true;
}

} // namespace evosim 