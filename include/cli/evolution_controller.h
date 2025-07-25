#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include "core/environment.h"
#include "core/evolution_engine.h"

namespace evosim {

/**
 * @brief Manages the lifecycle of the EvolutionEngine.
 * 
 * This controller is responsible for running the evolution simulation as a
 * background service (daemon). It initializes the engine, runs the simulation
 * in a dedicated thread, and provides a mechanism for graceful shutdown. It is
 * designed to be controlled by an external interface, such as a network server
 * or an interactive shell, but is not responsible for that interface itself.
 */
class EvolutionController {
public:
    /**
     * @brief Configuration for the core controller.
     */
    struct Config {
        std::string config_file;
        std::string log_file;
        bool enable_colors;
        bool enable_interactive;
        int server_port = 9090; ///< Port for the daemon to listen on.
    };

    /**
     * @brief Constructor
     * @param config The controller's configuration.
     */
    explicit EvolutionController(
        const Config& controller_config,
        const Environment::Config& env_config,
        const EvolutionEngine::Config& engine_config,
        const BytecodeVM::Config& vm_config,
        const SymmetryAnalyzer::Config& analyzer_config
    );

    /**
     * @brief Destructor that ensures clean shutdown.
     */
    ~EvolutionController();

    // Disable copy and move semantics to prevent slicing and ensure single ownership.
    EvolutionController(const EvolutionController&) = delete;
    EvolutionController& operator=(const EvolutionController&) = delete;
    EvolutionController(EvolutionController&&) = delete;
    EvolutionController& operator=(EvolutionController&&) = delete;

    /**
     * @brief Initializes the controller and the evolution engine.
     * @return True if initialization was successful.
     */
    bool initialize();

    /**
     * @brief Runs the controller as a background daemon.
     * 
     * This method starts the evolution engine in a background thread and then
     * enters a loop to listen for control commands (e.g., over a network socket).
     * This method will block until the server is shut down.
     * @return An exit code for the application.
     */
    int runAsDaemon();

private:
    /**
     * @brief Signals the server and the evolution loop to stop.
     */
    void stopServer();

    /**
     * @brief Handles a single client connection in a blocking manner.
     * 
     * This method reads a JSON request from the client, processes the command,
     * and sends a JSON response back.
     * @param socket The Boost.Asio socket for the connected client.
     */
    void handleClientConnection(boost::asio::ip::tcp::socket socket);

    Config config_;
    std::unique_ptr<EvolutionEngine> engine_;
    std::atomic<bool> is_running_{false};
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

} // namespace evosim 