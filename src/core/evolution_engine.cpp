#include "core/evolution_engine.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace evosim {

// --- JSON Serialization for EvolutionEngine::Config ---
// This function tells the nlohmann::json library how to convert our Config
// struct into a JSON object. It's found via Argument-Dependent Lookup (ADL).
void to_json(nlohmann::json& j, const EvolutionEngine::Config& c) {
    j = nlohmann::json{
        {"auto_start", c.auto_start},
        {"save_directory", c.save_directory},
        {"max_generations", c.max_generations},
        {"enable_save_state", c.enable_save_state},
        {"save_interval_generations", c.save_interval_generations},
        {"enable_backup", c.enable_backup},
        {"backup_interval", c.backup_interval},
        {"enable_metrics", c.enable_metrics},
        {"metrics_interval", c.metrics_interval},
        {"enable_logging", c.enable_logging}};
}   

EvolutionEngine::EvolutionEngine(EnvironmentPtr environment, const Config& config)
    : environment_(environment)
    , config_(config)
    , running_(false)
    , paused_(false)
    , should_stop_(false) {
    if (config_.auto_start) {
        start();
     }
 }

EvolutionEngine::~EvolutionEngine() {
    stop();
    if (evolution_thread_.joinable()) {
        evolution_thread_.join();
     }
 }

bool EvolutionEngine::start() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (running_) {
        return false; // Already running
     }

    // --- Automatic Resuming Logic ---
    const std::string checkpoint_file = config_.save_directory + "/checkpoint.json";
    if (std::filesystem::exists(checkpoint_file)) {
        spdlog::info("Checkpoint file found at '{}'. Attempting to resume.", checkpoint_file);
        if (loadState(checkpoint_file)) {
            spdlog::info("Successfully resumed from checkpoint. Starting evolution at generation {}.", stats_.total_generations);
        } else {
            spdlog::warn("Failed to load from checkpoint. Starting a new simulation.");
        }
    }

    logEffectiveConfig();

    try {
        running_ = true;
        paused_ = false;
        should_stop_ = false;
        
        stats_.start_time = Clock::now();
        stats_.is_running = true;
        stats_.is_paused = false;
        
        evolution_thread_ = std::thread(&EvolutionEngine::evolutionLoop, this);
        
        emitEvent({EventType::ENGINE_STARTED, 0, Clock::now(), "Evolution engine started", 0.0, 0});
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start evolution engine: {}", e.what());
        running_ = false;
        return false;
    }
}

bool EvolutionEngine::stop() {
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!running_) {
            return false; // Not running
        }
        should_stop_ = true;
        cv_.notify_all(); // Ensure the evolution thread wakes up if paused
    }

    // --- Prevent self-join deadlock ---
    // If stop() is called from the evolution thread itself, we must not join it.
    // The thread will exit its loop and terminate naturally.
    if (evolution_thread_.joinable() && std::this_thread::get_id() != evolution_thread_.get_id()) {
        evolution_thread_.join();
    }

    // The thread has now stopped (or will stop on its own).
    // The thread that called stop() is responsible for the final state cleanup.
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        running_ = false;
        paused_ = false;
        stats_.is_running = false;
        stats_.is_paused = false;
    }
    emitEvent({EventType::ENGINE_STOPPED, stats_.total_generations, Clock::now(), "Evolution engine stopped", 0.0, 0});
    return true;
}

bool EvolutionEngine::pause() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (!running_ || paused_) {
        return false;
    }
    
    paused_ = true;
    stats_.is_paused = true;
    
    emitEvent({EventType::ENGINE_PAUSED, stats_.total_generations, Clock::now(), "Evolution engine paused", 0.0, 0});
    
    return true;
}

bool EvolutionEngine::resume() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    if (!running_ || !paused_) {
        return false;
    }
    
    paused_ = false;
    stats_.is_paused = false;
    cv_.notify_all();
    
    emitEvent({EventType::ENGINE_RESUMED, stats_.total_generations, Clock::now(), "Evolution engine resumed", 0.0, 0});
    
    return true;
}

// Rename runGeneration to RunGeneration and make it private
bool EvolutionEngine::run_generation_() {
    // This check is safe to do without a lock because these are atomic or only
    // modified by the main thread when the loop is not running.
    if (!running_ || paused_.load() || should_stop_.load()) {
        return false;
    }

    // --- Perform long-running work WITHOUT holding the lock ---
    // The environment has its own internal mutex, so this call is thread-safe.
    // This is the key change to prevent deadlocks with getStats().
    if (environment_) {
        if (!environment_->update()) {
            std::lock_guard<std::recursive_mutex> lock(mutex_); // Lock only to emit the event
            emitEvent({EventType::ERROR_OCCURRED, stats_.total_generations, Clock::now(), "Environment update failed for the generation.", 0.0, 0});
            return false;
        }
    } else {
        return false; // No environment to update.
    }

    // --- Acquire lock only to update the engine's internal state ---
    try {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        // Re-check stop condition now that we have the lock, in case a stop was requested during the update.
        if (should_stop_.load()) {
            return false;
        }
        stats_.total_generations++;
        stats_.last_generation_time = Clock::now();
        emitEvent({EventType::GENERATION_COMPLETED, stats_.total_generations, Clock::now(), "Generation completed", 0.0, 0});
        updateStats();
        performPeriodicTasks(stats_.total_generations);

        // --- Check for max generations stopping criterion ---
        if (config_.max_generations > 0 && stats_.total_generations >= config_.max_generations) {
            spdlog::info("Reached max generations ({}), stopping evolution.", config_.max_generations);
            should_stop_ = true;
        }

        return true;
    } catch (const std::exception& e) {
        std::lock_guard<std::recursive_mutex> lock(mutex_); // Lock to emit the event
        emitEvent({EventType::ERROR_OCCURRED, stats_.total_generations, Clock::now(), "Error in generation: " + std::string(e.what()), 0.0, 0});
        return false;
    }
}

EvolutionEngine::EngineStats EvolutionEngine::getStats() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    EngineStats stats = stats_;
    
    // Update runtime
    auto now = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats_.start_time);
    stats.total_runtime_ms = static_cast<uint64_t>(duration.count());
    
    // Calculate generations per second
    if (stats.total_runtime_ms > 0) {
        stats.generations_per_second = static_cast<double>(stats_.total_generations) / (static_cast<double>(stats.total_runtime_ms) / 1000.0);
    }
    
    // Update current population and fitness info
    if (environment_) {
        // Fetch the pre-calculated stats from the environment for efficiency.
        auto env_stats = environment_->getStats();
        stats.current_population = env_stats.population_size;
        stats.current_best_fitness = env_stats.max_fitness;
        stats.current_avg_fitness = env_stats.avg_fitness;
    }
    
    return stats;
}

void EvolutionEngine::registerEventCallback(EventCallback callback) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    event_callback_ = callback;
}

void EvolutionEngine::unregisterEventCallback() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    event_callback_ = nullptr;
}

bool EvolutionEngine::saveState(const std::string& filename) {
    // Public-facing method. Acquire lock and call the internal implementation.
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return saveState_unlocked(filename);
}

bool EvolutionEngine::loadState(const std::string& filename) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    // Loading can only be done when the engine is stopped to prevent race conditions.
    if (running_) {
        spdlog::warn("Cannot load state while the engine is running. Please stop it first.");
        return false;
    }

    try {
        spdlog::debug("EvolutionEngine::loadState: Attempting to delegate loading to Environment...");
        // Delegate the complex task of loading the population to the environment.
        if (!environment_->loadState(filename)) {
            spdlog::warn("EvolutionEngine::loadState: environment->loadState returned false.");
            return false;
        }
        spdlog::debug("EvolutionEngine::loadState: Environment loading completed successfully.");

        // Reset engine-specific runtime stats, but sync with the loaded environment state.
        spdlog::debug("EvolutionEngine::loadState: Resetting engine stats and syncing from environment.");
        stats_ = {}; // Reset runtime, etc.
        stats_.start_time = Clock::now(); // A new "run" starts now.
        
        // Get the loaded stats from the environment
        spdlog::debug("EvolutionEngine::loadState: Fetching stats from environment...");
        auto env_stats = environment_->getStats();
        stats_.total_generations = env_stats.generation;
        stats_.current_population = env_stats.population_size;
        stats_.current_avg_fitness = env_stats.avg_fitness;
        stats_.current_best_fitness = env_stats.max_fitness;
        
        emitEvent({EventType::STATE_LOADED, stats_.total_generations, Clock::now(), "State loaded from " + filename, 0.0, 0});
        spdlog::debug("EvolutionEngine::loadState: State load finished.");
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception while loading state: {}", e.what());
        return false;
    }
}

bool EvolutionEngine::waitForCompletion(uint32_t timeout_ms) {
    auto start_time = Clock::now();
    
    while (running_ && !should_stop_) {
        auto now = Clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        
        if (timeout_ms > 0 && elapsed.count() >= timeout_ms) {
            return false; // Timeout
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return !running_;
}

std::deque<EvolutionEngine::Event> EvolutionEngine::getHistory() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return history_;
}

void EvolutionEngine::clearHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.clear();
}

bool EvolutionEngine::exportData(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (!file) {
            spdlog::error("Failed to open file for export: {}", filename);
            return false;
        }
        
        auto stats = getStats();
        nlohmann::json data;
        data["summary_stats"]["total_generations"] = stats.total_generations;
        data["summary_stats"]["total_runtime_ms"] = stats.total_runtime_ms;
        data["summary_stats"]["generations_per_second"] = stats.generations_per_second;
        data["summary_stats"]["current_population"] = stats.current_population;
        data["summary_stats"]["current_best_fitness"] = stats.current_best_fitness;
        data["summary_stats"]["current_avg_fitness"] = stats.current_avg_fitness;

        file << data.dump(4); // Pretty-print JSON with 4-space indent
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to export data to {}: {}", filename, e.what());
        return false;
    }
}

void EvolutionEngine::evolutionLoop() {
    while (true) {
        std::unique_lock<std::recursive_mutex> lock(mutex_);
        if (should_stop_) {
            break;
        }
        if (paused_) {
            cv_.wait(lock, [this] { return !paused_ || should_stop_; });
            if (should_stop_) {
                break;
            }
            continue;
        }
        lock.unlock();
        bool gen_result = run_generation_();
        if (!gen_result || should_stop_) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // The stop() method is now responsible for final state cleanup after joining
    // the thread, which prevents race conditions and clarifies ownership.
}

void EvolutionEngine::emitEvent(const Event& event) {
    if (event_callback_) {
        event_callback_(event);
    }
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.push_back(event);
    
    // Keep history size manageable, trim to 1000.
    // std::deque::pop_front() is an O(1) operation.
    while (history_.size() > 1000) {
        history_.pop_front();
    }
}

void EvolutionEngine::updateStats() {
    // Update basic stats
    auto now = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - stats_.start_time);
    stats_.total_runtime_ms = static_cast<uint64_t>(duration.count());
    
    if (stats_.total_runtime_ms > 0) {
        stats_.generations_per_second = static_cast<double>(stats_.total_generations) / (static_cast<double>(stats_.total_runtime_ms) / 1000.0);
    }
}

void EvolutionEngine::performPeriodicTasks(uint64_t generation) {
    // Save state if enabled
    if (config_.enable_save_state && generation % config_.save_interval_generations == 0) {
        // Save to a consistent checkpoint file for automatic resuming.
        saveState_unlocked(config_.save_directory + "/checkpoint.json");
    }
    
    // Save backup if enabled
    if (config_.enable_backup && generation % config_.backup_interval == 0) {
        saveBackup(generation);
    }
    
    // Collect metrics if enabled
    if (config_.enable_metrics && generation % config_.metrics_interval == 0) {
        collectMetrics();
    }
}

void EvolutionEngine::saveBackup(uint64_t generation) {
    // This function is called from performPeriodicTasks, which already holds the lock.
    // Therefore, it must call the unlocked version of saveState.
    const std::string filename = generateFilename("backup_" + std::to_string(generation), "json");
    saveState_unlocked(filename);
}

bool EvolutionEngine::saveState_unlocked(const std::string& filename) {
    // This private helper assumes the caller already holds the mutex.
    try {
        const std::string actual_filename = filename.empty() ? generateFilename("state", "json") : filename;

        if (!ensureSaveDirectory()) {
            return false;
        }

        // The lock held by the caller provides consistency. No need to pause/resume.
        // The environment's saveState is internally thread-safe.
        if (environment_->saveState(actual_filename)) {
            emitEvent({EventType::STATE_SAVED, stats_.total_generations, Clock::now(), "State saved to " + actual_filename, 0.0, 0});
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Exception while saving state: {}", e.what());
        return false;
    }
}

void EvolutionEngine::logEffectiveConfig() const {
    if (!config_.enable_logging) {
        return;
    }

    nlohmann::json full_config;
    full_config["evolution_engine"] = config_;
    if (environment_) {
        full_config["environment"] = environment_->getConfig();
        full_config["bytecode_vm"] = environment_->getVMConfig();
        full_config["symmetry_analyzer"] = environment_->getAnalyzerConfig();
    }

    spdlog::info("Starting evolution with the following effective configuration:\n{}", full_config.dump(4));
}

void EvolutionEngine::collectMetrics() {
    // TODO: Implement metrics collection
}

std::string EvolutionEngine::generateFilename(const std::string& prefix, const std::string& extension) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    // std::localtime is not thread-safe. We must protect access to it.
    // A static mutex is a simple way to ensure safety across all instances.
    static std::mutex time_mutex;
    std::lock_guard<std::mutex> lock(time_mutex);
    std::tm tm = *std::localtime(&time_t);

    std::ostringstream oss;
    oss << config_.save_directory << "/" << prefix << "_";
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    oss << "." << extension;
    
    return oss.str();
}

bool EvolutionEngine::ensureSaveDirectory() const {
    if (config_.save_directory.empty()) {
        return true;
    }
    
    try {
        std::filesystem::create_directories(config_.save_directory);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create save directory '{}': {}", config_.save_directory, e.what());
        return false;
    }
}

} // namespace evosim 
