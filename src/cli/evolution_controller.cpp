#include "cli/evolution_controller.h"
#include "core/evolution_engine.h" // Full definition is required for std::unique_ptr
#include "core/environment.h"      // Required for creating the environment
#include "utils/logger.h"

// This implementation will require a JSON library.
// A common choice is nlohmann/json, which is header-only.
#include "nlohmann/json.hpp"

#include <chrono>
#include <iostream>
#include <sys/socket.h> // For read/write on sockets
#include <unistd.h>     // For close()

// for convenience
using json = nlohmann::json;

namespace evosim {

EvolutionController::EvolutionController(const Config& config) : config_(config) {
    // The EvolutionEngine requires an Environment to be constructed.
    // We create it here and pass it to the engine's constructor.
    auto environment = std::make_shared<Environment>();
    engine_ = std::make_unique<EvolutionEngine>(environment);
}

EvolutionController::~EvolutionController() {
    if (is_running_.load()) {
        stopServer();
    }
    // Ensure the thread is joined for a clean shutdown
    if (evolution_thread_.joinable()) {
        evolution_thread_.join();
    }
    if (server_socket_ != -1) {
        close(server_socket_);
    }
}

bool EvolutionController::initialize() {
    evosim::log_info("EvolutionController initialized.");
    // TODO: Initialize engine_ with settings from config_
    return true;
}

int EvolutionController::runAsDaemon() {
    evosim::log_info("runAsDaemon is a placeholder. TCP server logic will be added here.");
    // In the full implementation, this will create a listening socket
    // and loop on accept(), calling handleClientConnection for each client.

    // For now, let's just start the background thread and wait for it to finish.
    is_running_ = true;
    evosim::log_info("Starting evolution engine in a background thread...");
    evolution_thread_ = std::thread(&EvolutionController::runEvolutionLoop, this);

    if (evolution_thread_.joinable()) {
        evolution_thread_.join();
    }
    return 0;
}

void EvolutionController::runEvolutionLoop() {
    evosim::log_info("Evolution loop thread started.");
    while (is_running_.load()) {
        // engine_->step(); // This would be the actual call
        evosim::log_debug("Evolution engine step...");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    evosim::log_info("Evolution loop thread has stopped.");
}

void EvolutionController::stopServer() {
    evosim::log_info("Stop signal received. Shutting down server...");
    is_running_ = false;
}

void EvolutionController::handleClientConnection(int client_socket) {
    evosim::log_debug("Handling new client connection on socket " + std::to_string(client_socket));

    char buffer[1024] = {0};
    json response;

    // 1. Read request from client (simplified for now)
    if (read(client_socket, buffer, sizeof(buffer) - 1) <= 0) {
        evosim::log_warn("Failed to read from client socket or client disconnected.");
        close(client_socket);
        return;
    }

    // 2. Parse request and process command
    try {
        json request = json::parse(buffer);
        std::string command = request.at("command");
        evosim::log_info("Received command: '" + command + "'");

        if (command == "status") {
            response = {{"status", "ok"}, {"data", {{"engine_state", "running"}, {"generation", 42}}}};
        } else if (command == "stop") {
            response = {{"status", "ok"}, {"message", "Shutdown signal sent."}};
            stopServer();
        } else {
            response = {{"status", "error"}, {"message", "Unknown command: " + command}};
        }

    } catch (const json::parse_error& e) {
        response = {{"status", "error"}, {"message", "Invalid JSON request."}};
        evosim::log_error("JSON parse error: " + std::string(e.what()));
    } catch (const json::out_of_range& e) {
        response = {{"status", "error"}, {"message", "Request missing 'command' field."}};
        evosim::log_error("JSON format error: " + std::string(e.what()));
    }

    // 3. Send response and close connection
    std::string response_str = response.dump(4); // pretty-print JSON
    write(client_socket, response_str.c_str(), response_str.length());
    close(client_socket);
    evosim::log_debug("Client connection closed.");
}

} // namespace evosim