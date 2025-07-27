#include "cli/evolution_controller.h"
#include "core/evolution_engine.h" // Full definition is required for std::unique_ptr
#include "core/environment.h"      // Required for creating the environment
#include "spdlog/spdlog.h"

// This implementation will require a JSON library.
// A common choice is nlohmann/json, which is header-only.
#include "nlohmann/json.hpp"

#include <chrono>
#include <cstring>      // For memset
#include <iostream>
#include <filesystem>   // For creating directories
#include <sstream>      // For building filenames
#include <iomanip>      // For std::setprecision
#include "opencv2/imgcodecs.hpp" // For cv::imwrite

// for convenience
using json = nlohmann::json;
using boost::asio::ip::tcp;

namespace evosim {

// Add a to_json overload for EngineStats to enable automatic serialization.
void to_json(json& j, const EvolutionEngine::EngineStats& stats) {
    j = json{
        {"is_running", stats.is_running},
        {"is_paused", stats.is_paused},
        {"total_generations", stats.total_generations},
        {"total_runtime_ms", stats.total_runtime_ms},
        {"generations_per_second", stats.generations_per_second},
        {"current_population", stats.current_population},
        {"current_best_fitness", stats.current_best_fitness},
        {"current_avg_fitness", stats.current_avg_fitness}
    };
}

EvolutionController::EvolutionController(
    const Config& controller_config,
    const Environment::Config& env_config,
    const EvolutionEngine::Config& engine_config,
    const BytecodeVM::Config& vm_config,
    const SymmetryAnalyzer::Config& analyzer_config
) : config_(controller_config), acceptor_(io_context_) {
    // Create the core components using the configurations loaded from the YAML file.
    auto environment = std::make_shared<Environment>(env_config, vm_config, analyzer_config);
    engine_ = std::make_unique<EvolutionEngine>(environment, engine_config);
}

EvolutionController::~EvolutionController() {
    // If the acceptor is still open, it means shutdown wasn't clean.
    if (acceptor_.is_open()) {
        acceptor_.close();
    }
}

bool EvolutionController::initialize() {
    // This method can be used for post-construction initialization if needed.
    spdlog::info("EvolutionController initialized.");
    return true;
}

int EvolutionController::runAsDaemon() {
    try {
        tcp::endpoint endpoint(tcp::v4(), static_cast<unsigned short>(config_.server_port));

        // 1. Open, set options, bind, and listen on the acceptor socket
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();

        spdlog::info("Server listening on port {}", config_.server_port);

        // 1. Start the evolution engine. This will either start a new simulation
        // or resume from a checkpoint if one exists.
        if (!engine_->start()) {
            spdlog::error("Failed to start the evolution engine.");
            return 1;
        }

        // 2. Save initial phenotypes ONLY if this is a new run (generation 0).
        // This must be done AFTER starting the engine to know if we resumed.
        if (config_.save_initial_phenotypes > 0) {
            if (engine_->getStats().total_generations == 0) {
                saveInitialPhenotypes(config_.save_initial_phenotypes);
            } else {
                spdlog::info("Skipping --save-initial-phenotypes because we resumed from a checkpoint at generation {}.", engine_->getStats().total_generations);
            }
        }

        is_running_ = true;

        // 3. Accept client connections in a loop
        while (is_running_.load()) {
            tcp::socket socket(io_context_);
            boost::system::error_code ec;
            acceptor_.accept(socket, ec);

            if (ec) {
                // An error occurred. If we are shutting down, this is expected.
                if (ec == boost::asio::error::operation_aborted) {
                    spdlog::info("Acceptor closed, shutting down server loop.");
                } else {
                    spdlog::error("Accept error: {}", ec.message());
                }
                break; // Exit the loop on any accept error
            }
            
            // Handle the client connection (synchronously for now)
            handleClientConnection(std::move(socket));
        }

    } catch (const std::exception& e) {
        spdlog::error("Server error in runAsDaemon: {}", e.what());
        is_running_ = false;
    }

    spdlog::info("Server loop has exited. Cleaning up.");
    engine_->stop(); // Ensure engine is stopped
    if (acceptor_.is_open()) {
        acceptor_.close();
    }
    return 0;
}

void EvolutionController::stopServer() {
    spdlog::info("Stop signal received. Shutting down server...");
    if (is_running_.exchange(false)) { // Ensure this runs only once
        // Closing the acceptor will unblock the `acceptor_.accept()` call
        // in the server loop, allowing it to terminate gracefully.
        acceptor_.close();
    }
}

void EvolutionController::handleClientConnection(tcp::socket socket) {
    spdlog::debug("Handling new client connection from {}", socket.remote_endpoint().address().to_string());

    boost::asio::streambuf buffer;
    json response;

    // 1. Read request from client (simplified for now)
    boost::system::error_code ec;
    boost::asio::read_until(socket, buffer, "\n", ec);
    if (ec && ec != boost::asio::error::eof) {
        spdlog::warn("Failed to read from client socket: {}", ec.message());
        return;
    }

    // 2. Parse request and process command
    try {
        // Create an istream from the streambuf to safely extract the line.
        // This correctly handles the buffer and delimiter.
        std::istream is(&buffer);
        std::string line;
        std::getline(is, line);
        json request = json::parse(line);
        std::string command = request.at("command");
        spdlog::info("Received command: '{}'", command);

        if (command == "status") {
            response["status"] = "ok";
            response["data"] = engine_->getStats(); // Uses the to_json overload
        } else if (command == "stats") { // Add alias for stats
            response["status"] = "ok";
            response["data"] = engine_->getStats();
        } else if (command == "stop") {
            response = {{"status", "ok"}, {"message", "Shutdown signal sent."}};
            stopServer();
        } else if (command == "pause") {
            bool success = engine_->pause();
            response["status"] = success ? "ok" : "error";
            response["message"] = success ? "Engine paused." : "Failed to pause engine (maybe not running or already paused).";
        } else if (command == "resume") {
            bool success = engine_->resume();
            response["status"] = success ? "ok" : "error";
            response["message"] = success ? "Engine resumed." : "Failed to resume engine (maybe not running or not paused).";
        } else if (command == "save") {
            std::string filename = request.value("file", ""); // Use .value() for optional fields
            bool success = engine_->saveState(filename);
            response["status"] = success ? "ok" : "error";
            response["message"] = success ? "State saved." : "Failed to save state.";
        } else if (command == "load") {
            if (!request.contains("file")) {
                response = {{"status", "error"}, {"message", "Load command requires a --file argument."}};
            } else {
                std::string filename = request.at("file");
                bool success = engine_->loadState(filename);
                response["status"] = success ? "ok" : "error";
                response["message"] = success ? "State loaded." : "Failed to load state. Is the engine stopped?";
            }
        } else if (command == "top") {
            uint32_t count = request.value("count", 5U);
            std::string output_dir = request.value("output_dir", "top_organisms");

            try {
                std::filesystem::create_directories(output_dir);
                auto env = engine_->getEnvironment();
                if (!env) {
                    response = {{"status", "error"}, {"message", "Environment not available."}};
                } else {
                    auto top_organisms = env->getTopFittest(count);
                    BytecodeVM temp_vm(env->getVMConfig());
                    std::vector<std::string> saved_files;

                    for (const auto& org : top_organisms) {
                        auto image = temp_vm.execute(org->getBytecode());
                        auto stats = org->getStats();
                        std::stringstream ss;
                        ss << output_dir << "/organism_" << stats.id << "_fit_" << std::fixed << std::setprecision(4) << stats.fitness_score << ".png";
                        std::string filepath = ss.str();
                        cv::imwrite(filepath, image);
                        saved_files.push_back(filepath);
                    }
                    response["status"] = "ok";
                    response["files"] = saved_files;
                }
            } catch (const std::exception& e) {
                response = {{"status", "error"}, {"message", "Failed to generate images: " + std::string(e.what())}};
            }
        } else if (command == "generate-test-phenotype") {
            uint32_t width = request.value("width", 256U);
            uint32_t height = request.value("height", 256U);
            std::string filepath = generateTestPhenotype(width, height);
            if (!filepath.empty()) {
                response["status"] = "ok";
                response["message"] = "Test phenotype saved to " + filepath;
                response["file"] = filepath;
            } else {
                response["status"] = "error";
                response["message"] = "Failed to generate or save test phenotype.";
            }
        } else {
            response = {{"status", "error"}, {"message", "Unknown command: " + command}};
        }

    } catch (const json::parse_error& e) {
        response = {{"status", "error"}, {"message", "Invalid JSON request."}};
        spdlog::error("JSON parse error: {}", e.what());
    } catch (const json::out_of_range& e) {
        response = {{"status", "error"}, {"message", "Request missing 'command' field."}};
        spdlog::error("JSON format error: {}", e.what());
    }

    // 3. Send response and close connection
    std::string response_str = response.dump() + "\n"; // Add newline delimiter
    boost::asio::write(socket, boost::asio::buffer(response_str), ec);
    // The socket will be closed automatically by its destructor when it goes out of scope.
    spdlog::debug("Client connection handled.");
}

void EvolutionController::saveInitialPhenotypes(uint32_t count) {
    spdlog::info("Saving {} initial phenotypes for inspection...", count);
    auto env = engine_->getEnvironment();
    if (!env) {
        spdlog::error("Cannot save initial phenotypes: Environment is not available.");
        return;
    }

    // We need a VM to execute bytecode. Get one with the correct configuration from the environment.
    BytecodeVM temp_vm(env->getVMConfig());
    
    const std::string output_dir = "initial_phenotypes";
    try {
        std::filesystem::create_directories(output_dir);
        
        auto population = env->getPopulation(); // This is a map of the initial population
        uint32_t saved_count = 0;
        for (const auto& pair : population) {
            if (saved_count >= count) break;

            const auto& org = pair.second;
            auto image = temp_vm.execute(org->getBytecode());
            
            std::stringstream ss;
            ss << output_dir << "/initial_organism_" << org->getStats().id << ".png";
            std::string filepath = ss.str();
            
            if (cv::imwrite(filepath, image)) {
                saved_count++;
            } else {
                spdlog::warn("Failed to write image for initial organism {}", org->getStats().id);
            }
        }
        spdlog::info("Successfully saved {} phenotypes to '{}' directory.", saved_count, output_dir);
    } catch (const std::exception& e) {
        spdlog::error("An exception occurred while saving initial phenotypes: {}", e.what());
    }
}

std::string EvolutionController::generateTestPhenotype(uint32_t width, uint32_t height) {
    spdlog::debug("Generating a {}x{} test phenotype with random pixels.", width, height);
    try {
        // Create a 3-channel (color) image of the specified size.
        cv::Mat image(static_cast<int>(height), static_cast<int>(width), CV_8UC3);

        // Fill the image with uniformly distributed random pixel values.
        cv::randu(image, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));

        const std::string filepath = "test_phenotype_random_pixels.png";
        if (cv::imwrite(filepath, image)) {
            spdlog::info("Successfully saved test phenotype to '{}'", filepath);
            return filepath;
        }
        spdlog::error("Failed to write test phenotype image to '{}'", filepath);
    } catch (const std::exception& e) {
        spdlog::error("Exception while generating test phenotype: {}", e.what());
    }
    return "";
}

} // namespace evosim