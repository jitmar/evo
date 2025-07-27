#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <boost/program_options.hpp>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "cli/evolution_controller.h"
#include "utils/config_manager.h"
#include "utils/random_generator.h"

namespace po = boost::program_options;

/**
 * @brief Print program banner
 */
void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                  EVOSIMD - EvoSim Daemon                     ║
║                                                              ║
║  The background server process for the Evolution Simulator.  ║
║                                                              ║
║  Version: 0.1.0                                              ║
║  License: GPL v3.0                                           ║
╚══════════════════════════════════════════════════════════════╝
)" << '\n';
}

/**
 * @brief Print daemon usage
 */
void printUsage(const po::options_description& desc) {
    std::cout << "Usage: evosimd [options]\n\n";
    std::cout << "Starts the EvoSim server daemon. This process runs in the foreground\n";
    std::cout << "and listens for commands from the 'evosim' client.\n\n";
    std::cout << "Options:\n";
    std::cout << desc << "\n";
}

/**
 * @brief Parse command line arguments for the daemon
 */
bool parseArguments(int argc, char* argv[], po::variables_map& vm, po::options_description& desc) {
    desc.add_options()
        ("help,h", "Show this help message")
        ("config,c", po::value<std::string>(), "Configuration file (e.g., evosim.yaml)")
        ("log-level,l", po::value<std::string>()->default_value("info"), "Log level (trace, debug, info, etc.)")
        ("log-file", po::value<std::string>(), "Log file path")
        ("seed", po::value<uint64_t>(), "Random seed");

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
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
    try {
        std::vector<spdlog::sink_ptr> sinks;
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%^%l%$] %v");
        sinks.push_back(console_sink);

        if (vm.count("log-file")) {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(vm["log-file"].as<std::string>(), true);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
            sinks.push_back(file_sink);
        }

        auto logger = std::make_shared<spdlog::logger>("evosimd", begin(sinks), end(sinks));
        logger->set_level(spdlog::level::from_str(vm["log-level"].as<std::string>()));
        spdlog::set_default_logger(logger);
        // --- FIX: Flush on debug to see messages immediately before a hang ---
        // By default, logs are buffered. Flushing on 'info' means we won't see 'debug'
        // messages if the program hangs before the next 'info' log.
        spdlog::flush_on(spdlog::level::debug);

    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        return false;
    }
    return true;
}

/**
 * @brief Main function for the server daemon
 */
int main(int argc, char* argv[]) {
    try {
        po::options_description desc("Allowed options");
        po::variables_map vm;
        if (!parseArguments(argc, argv, vm, desc)) {
            return 1;
        }

        if (vm.count("help")) {
            printBanner();
            printUsage(desc);
            return 0;
        }

        if (!initializeLogging(vm)) {
            std::cerr << "Fatal: Failed to initialize logging system." << std::endl;
            return 1;
        }

        spdlog::info("EvoSim Daemon starting up...");

        if (vm.count("seed")) {
            uint64_t seed = vm["seed"].as<uint64_t>();
            evosim::RandomGenerator::setGlobalSeed(seed);
            spdlog::info("Global random seed set to: {}", seed);
        }

        std::string config_file = vm.count("config") ? vm["config"].as<std::string>() : "evosim.yaml";

        evosim::ConfigManager config_manager(config_file);
        if (!config_manager.load()) {
            spdlog::error("Failed to load or parse configuration file '{}'. Exiting.", config_file);
            return 1;
        }

        evosim::EvolutionController::Config controller_config;
        controller_config.config_file = config_file;

        auto env_config = config_manager.getEnvironmentConfig();
        auto engine_config = config_manager.getEvolutionEngineConfig();
        auto vm_config = config_manager.getBytecodeVMConfig();
        auto analyzer_config = config_manager.getSymmetryAnalyzerConfig();

        evosim::EvolutionController controller(
            controller_config, env_config, engine_config, vm_config, analyzer_config
        );

        if (!controller.initialize()) {
            spdlog::error("Failed to initialize evolution controller");
            return 1;
        }

        return controller.runAsDaemon();

    } catch (const std::exception& e) {
        spdlog::critical("A fatal error occurred in the daemon: {}", e.what());
        return 1;
    }
}