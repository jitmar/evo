#include <iostream>
#include <string>
#include <vector>
#include <iomanip> // For std::setw
#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#include "nlohmann/json.hpp"

namespace po = boost::program_options;

/**
 * @brief Print program banner
 */
void printBanner() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║                     EVOSIM - Client Utility                  ║
║                                                              ║
║  A command-line client for interacting with the evosimd      ║
║  server daemon.                                              ║
║                                                              ║
║  Version: 0.1.0                                              ║
║  License: GPL v3.0                                           ║
╚══════════════════════════════════════════════════════════════╝
)" << '\n';
}

/**
 * @brief A simple structure to define CLI commands for help text generation.
 */
struct CommandInfo {
    std::string name;
    std::string description;
};

/**
 * @brief Print program usage
 */
void printUsage(const po::options_description& desc) {
    const std::vector<CommandInfo> commands = {
        {"stop", "Stop the running evolution server."},
        {"pause", "Pause the simulation."},
        {"resume", "Resume a paused simulation."},
        {"status", "Show the current status of the simulation (alias: `stats`)."},
        {"save", "Request the server to save the current state. Use --file <path> for a custom name."},
        {"load", "Request the server to load state. Requires --file <path>."},
        {"top", "Generate images for the top N fittest organisms. Use --count N and --output-dir <path>."},
        {"help", "Show this help message."}
    };

    std::cout << "Usage: evosim [options] <command>\n\n";
    std::cout << "Commands:\n";
    for (const auto& cmd : commands) {
        std::cout << "  " << std::left << std::setw(10) << cmd.name << cmd.description << "\n";
    }
    std::cout << "\nOptions:\n";
    std::cout << desc << "\n";
    std::cout << "\nExamples:\n";
    std::cout << "  evosim status\n";
    std::cout << "  evosim save --file my_state.json\n";
}

/**
 * @brief Parse command line arguments
 */
bool parseArguments(int argc, char* argv[], po::variables_map& vm, po::options_description& desc) {
    desc.add_options()
        ("help,h", "Show help message")
        ("version,v", "Show version information")
        ("file,f", po::value<std::string>(), "File path for save/load operations")
        ("count,n", po::value<uint32_t>(), "Number of items for commands like 'top'")
        ("output-dir,o", po::value<std::string>(), "Output directory for generated files");

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
 * @brief Placeholder for client logic to send commands to the server.
 */
bool sendCommandToServer(const std::string& command, const po::variables_map& vm) {
    using boost::asio::ip::tcp;
    using json = nlohmann::json;

    try {
        boost::asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);

        // The default port is 9090, defined in EvolutionController::Config.
        // A more advanced client could get this from a config file.
        boost::asio::connect(socket, resolver.resolve("localhost", "9090"));

        // Construct the JSON request
        json request = {{"command", command}};
        if (vm.count("file")) {
            request["file"] = vm["file"].as<std::string>();
        }
        if (vm.count("count")) {
            request["count"] = vm["count"].as<uint32_t>();
        }
        if (vm.count("output-dir")) {
            request["output_dir"] = vm["output-dir"].as<std::string>();
        }

        // Send the request with a newline delimiter
        std::string request_str = request.dump() + "\n";
        boost::asio::write(socket, boost::asio::buffer(request_str));

        // Read the response
        boost::asio::streambuf response_buffer;
        boost::asio::read_until(socket, response_buffer, "\n");

        // Parse and display the response
        std::string response_data(boost::asio::buffer_cast<const char*>(response_buffer.data()), response_buffer.size());
        json response_json = json::parse(response_data);

        if (response_json.value("status", "error") == "ok") {
            std::cout << response_json.dump(4) << std::endl; // Pretty-print JSON
            return true;
        } else {
            std::cerr << "Server returned an error:\n" << response_json.dump(4) << std::endl;
            return false;
        }

    } catch (const boost::system::system_error& e) {
        if (e.code() == boost::asio::error::connection_refused) {
            // This is a common, expected error if the server isn't running.
        } else {
            std::cerr << "Communication error: " << e.what() << std::endl;
        }
        return false;
    }
}

/**
 * @brief Main function
 */
int main(int argc, char* argv[]) {
    try {
        // Define all command-line options in one place.
        po::options_description desc("Allowed options");

        // Parse command line arguments
        po::variables_map vm;
        if (!parseArguments(argc, argv, vm, desc)) {
            return 1;
        }
        
        // Show help
        if (vm.count("help")) {
            printBanner();
            printUsage(desc);
            return 0;
        }
        
        // Show version
        if (vm.count("version")) {
            printBanner();
            return 0;
        }
        
        // This executable is now a pure client. It requires a command.
        if (vm.count("command")) {
            const std::string command = vm["command"].as<std::string>();

            // Handle 'help' as a command, not just an option.
            if (command == "help") {
                printBanner();
                printUsage(desc);
                return 0;
            }

            // The 'start' command is now handled by the 'evosimd' executable.
            if (command == "start") {
                std::cerr << "Error: The 'start' command is handled by the 'evosimd' executable.\n"
                          << "Please run 'evosimd' to start the server." << std::endl;
                return 1;
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
            std::cout << "No command provided.\n\n";
            printUsage(desc);
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