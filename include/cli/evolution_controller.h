#pragma once

#include <memory>
#include <string>
#include <thread>
#include <atomic>

// Forward declaration for EvolutionEngine to reduce header dependencies.
// The full definition will be included in the .cpp file.
namespace evosim {
class EvolutionEngine;
}

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
    explicit EvolutionController(const Config& config);

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
     * @brief The main loop for the evolution engine, run in a separate thread.
     */
    void runEvolutionLoop();

    /**
     * @brief Signals the server and the evolution loop to stop.
     */
    void stopServer();

    /**
     * @brief Handles a single client connection in a blocking manner.
     * 
     * This method reads a JSON request from the client, processes the command,
     * and sends a JSON response back.
     * @param client_socket The socket descriptor for the connected client.
     */
    void handleClientConnection(int client_socket);

    Config config_;
    std::unique_ptr<EvolutionEngine> engine_;
    std::thread evolution_thread_;
    std::atomic<bool> is_running_{false};
    int server_socket_ = -1; ///< The listening socket for the daemon.
};

} // namespace evosim 