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
║  featuring self-replicating programs with symmetry-based    ║
║  fitness evaluation and bytecode virtual machines.          ║
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
    std::cout << "Usage: evosim [options] [command]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  start     Start evolution simulation\n";
    std::cout << "  stop      Stop evolution simulation\n";
    std::cout << "  pause     Pause evolution simulation\n";
    std::cout << "  resume    Resume evolution simulation\n";
    std::cout << "  status    Show simulation status\n";
    std::cout << "  stats     Show detailed statistics\n";
    std::cout << "  config    Show/modify configuration\n";
    std::cout << "  save      Save current state\n";
    std::cout << "  load      Load state from file\n";
    std::cout << "  export    Export evolution data\n";
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
    
    std::cout << "EvoSim Evolution Simulator starting up" << std::endl;
    std::cout << "Log level: " << log_level << std::endl;
    
    return true;
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
            return 1;
        }
        
        // Set random seed
        if (vm.count("seed")) {
            uint64_t seed = vm["seed"].as<uint64_t>();
            evosim::RandomGenerator::setGlobalSeed(seed);
            std::cout << "Random seed set to: " << seed << std::endl;
        }
        
        // Create evolution controller
        evosim::EvolutionController::Config controller_config;
        controller_config.enable_interactive = vm.count("interactive");
        controller_config.enable_auto_save = true;
        controller_config.config_file = "evosim.conf";
        controller_config.log_file = "evosim.log";
        controller_config.enable_colors = !vm.count("no-colors");
        controller_config.enable_progress = true;
        controller_config.max_history = 1000;
        controller_config.enable_completion = true;
        
        evosim::EvolutionController controller(controller_config);
        
        // Initialize controller
        if (!controller.initialize()) {
            std::cerr << "Failed to initialize evolution controller" << std::endl;
            return 1;
        }
        
        // Execute command or run interactively
        if (vm.count("command")) {
            std::string command = vm["command"].as<std::string>();
            std::cout << "Executing command: " << command << std::endl;
            
            if (!controller.processCommand(command)) {
                std::cerr << "Failed to execute command: " << command << std::endl;
                return 1;
            }
        } else if (controller.getConfig().enable_interactive) {
            // Run in interactive mode
            std::cout << "Starting interactive mode" << std::endl;
            return controller.runInteractive();
        } else {
            // Run with command line arguments
            std::cout << "Starting non-interactive mode" << std::endl;
            return controller.run(argc, argv);
        }
        
        std::cout << "EvoSim completed successfully" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred" << std::endl;
        return 1;
    }
} 