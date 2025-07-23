#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include "cli/evolution_controller.h"
#include "utils/logger.h"
#include "utils/config_manager.h"
#include "utils/random_generator.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;

/**
 * @brief Print program banner
 */
void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                    EVOSIM - Evolution Simulator              ║
║                                                              ║
║  A virtual environment for programmable organism evolution   ║
║  featuring self-replicating programs with symmetry-based     ║
║  fitness evaluation and bytecode virtual machines.           ║
║                                                              ║
║  Version: 0.1.0                                              ║
║  License: GPL v3.0                                           ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

/**
 * @brief Print program usage
 */
void printUsage() {
    std::cout << "Usage: evosim [options] <command>\n\n";
    std::cout << "Commands:\n";
    std::cout << "  start     Start the evolution simulation server in the background.\n";
    std::cout << "  stop      Stop the running evolution server.\n";
    std::cout << "  pause     Pause the simulation.\n";
    std::cout << "  resume    Resume a paused simulation.\n";
    std::cout << "  status    Show the current status of the simulation.\n";
    std::cout << "  stats     Show detailed statistics from the simulation.\n";
    std::cout << "  config    View or modify the server's configuration.\n";
    std::cout << "  save      Request the server to save the current state.\n";
    std::cout << "  load      Request the server to load state from a file.\n";
    std::cout << "  help      Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  evosim --config evosim.conf start\n";
    std::cout << "  evosim --log-level debug --interactive\n";
    std::cout << "  evosim --load-state save_001.evo status\n";
}

/**
 * @brief Parse command line arguments
 */
bool parseArguments(int argc, char* argv[], po::variables_map& vm) {
    po::options_description desc("EvoSim - Evolution Simulator Options");
    
    desc.add_options()
        ("help,h", "Show help message")
        ("version,v", "Show version information")
        ("config,c", po::value<std::string>(), "Configuration file")
        ("log-level,l", po::value<std::string>()->default_value("info"), 
         "Log level (trace, debug, info, warning, error, fatal)")
        ("log-file", po::value<std::string>(), "Log file path")
        ("interactive,i", "Run in interactive mode")
        ("non-interactive", "Run in non-interactive mode")
        ("seed", po::value<uint64_t>(), "Random seed")
        ("quiet,q", "Suppress output")
        ("verbose", "Verbose output")
        ("debug", "Enable debug mode")
        ("no-colors", "Disable colored output");

    po::options_description hidden("Hidden options");
    hidden.add_options()
        ("command", po::value<std::string>(), "Command to execute");

    po::positional_options_description pos;
    pos.add("command", 1);

    po::options_description all("All options");
    all.add(desc).add(hidden);

    try {
        po::store(po::command_line_parser(argc, argv)
                  .options(all)
                  .positional(pos)
                  .run(), vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error parsing command line arguments: " << e.what() << std::endl;
        return false;
    }

    return true;
}

/**
 * @brief Initialize logging system
 */
bool initializeLogging(const po::variables_map& vm) {
    evosim::LoggerConfig config;
    
    // Set log level
    std::string log_level = vm["log-level"].as<std::string>();
    config.level = evosim::Logger::parseLevel(log_level);
    
    // Set log file
    if (vm.count("log-file")) {
        config.filename = vm["log-file"].as<std::string>();
        config.enable_file = true;
    }
    
    // Set console output
    config.enable_console = !vm.count("quiet");
    
    // Set colored output
    config.enable_colors = !vm.count("no-colors");
    
    // Initialize logger
    evosim::initializeLogger(config);
    return true;
}

/**
 * @brief Create controller configuration from command line arguments
 */
evosim::EvolutionController::Config createControllerConfig(const po::variables_map& vm) {
    evosim::EvolutionController::Config config;

    if (vm.count("config")) {
        config.config_file = vm["config"].as<std::string>();
    } else {
        config.config_file = "evosim.conf"; // Default
    }

    if (vm.count("log-file")) {
        config.log_file = vm["log-file"].as<std::string>();
    }

    config.enable_colors = !vm.count("no-colors");
    config.enable_interactive = vm.count("interactive");

    return config;
}

/**
 * @brief Placeholder for client logic to send commands to the server.
 */
bool sendCommandToServer(const std::string& command, const po::variables_map& vm) {
    // In a real implementation, this function would use a client library
    // (e.g., for TCP sockets or HTTP) to connect to the running daemon
    // and send the command and its arguments.
    // For now, it's a placeholder.
    std::cout << "[Client] Connecting to server to send command: '" << command << "'..." << std::endl;

    // TODO: Implement IPC client logic here.
    std::cerr << "[Client] IPC not yet implemented. Cannot connect to server." << std::endl;
    
    return false; // Return false to indicate failure until implemented.
}

/**
 * @brief Main function
 */
int main(int argc, char* argv[]) {
    try {
        // Parse command line arguments
        po::variables_map vm;
        if (!parseArguments(argc, argv, vm)) {
            return 1;
        }
        
        // Show help
        if (vm.count("help")) {
            printBanner();
            printUsage();
            return 0;
        }
        
        // Show version
        if (vm.count("version")) {
            printBanner();
            return 0;
        }
        
        // Initialize logging
        if (!initializeLogging(vm)) {
            // Can't use logger here, so std::cerr is appropriate
            std::cerr << "Fatal: Failed to initialize logging system." << std::endl;
            return 1;
        }
        
        evosim::log_info("EvoSim Evolution Simulator starting up");
        evosim::log_info("Log level set to: " + vm["log-level"].as<std::string>());

        // Set random seed
        if (vm.count("seed")) {
            uint64_t seed = vm["seed"].as<uint64_t>();
            evosim::RandomGenerator::setGlobalSeed(seed);
            evosim::log_info("Global random seed set to: " + std::to_string(seed));
        }
        
        // Create evolution controller
        auto controller_config = createControllerConfig(vm);
        evosim::EvolutionController controller(controller_config);
        
        // Initialize controller
        if (!controller.initialize()) {
            evosim::log_error("Failed to initialize evolution controller");
            return 1;
        }
        
        // Execute command or run interactively
        if (vm.count("command")) {
            const std::string command = vm["command"].as<std::string>();

            if (command == "start") {
                // --- SERVER PATH ---
                // The 'start' command launches the main evolution controller as a daemon.
                evosim::log_info("Starting EvoSim server...");
                // runAsDaemon() will contain the server loop that listens for client commands.
                // This method will block until a 'stop' command is received.
                return controller.runAsDaemon();
            } else {
                // --- CLIENT PATH ---
                // All other commands are sent to the running server process.
                if (sendCommandToServer(command, vm)) {
                    return 0;
                } else {
                    std::cerr << "Failed to communicate with EvoSim server. Is it running?" << std::endl;
                    return 1;
                }
            }
        } else {
            printBanner();
            printUsage();
        }

        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
} 