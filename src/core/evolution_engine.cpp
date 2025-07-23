#include "core/evolution_engine.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace evosim {

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
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        return false; // Already running
    }
    
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
        running_ = false;
        return false;
    }
}

bool EvolutionEngine::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return false; // Not running
        }
        should_stop_ = true;
        cv_.notify_all(); // Ensure the evolution thread wakes up if paused
    }
    if (evolution_thread_.joinable()) {
        evolution_thread_.join();
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        paused_ = false;
        stats_.is_running = false;
        stats_.is_paused = false;
    }
    emitEvent({EventType::ENGINE_STOPPED, stats_.total_generations, Clock::now(), "Evolution engine stopped", 0.0, 0});
    return true;
}

bool EvolutionEngine::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_ || paused_) {
        return false;
    }
    
    paused_ = true;
    stats_.is_paused = true;
    
    emitEvent({EventType::ENGINE_PAUSED, stats_.total_generations, Clock::now(), "Evolution engine paused", 0.0, 0});
    
    return true;
}

bool EvolutionEngine::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    
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
    std::lock_guard<std::mutex> lock(mutex_);

    // Removed logic_error exception: this method is now private and only called internally.
    if (!running_ || paused_ || should_stop_) {
        return false;
    }
    try {
        // Run one generation
        if (environment_) {
            auto organisms = environment_->getPopulation();
            for (size_t i = 0; i < organisms.size(); ++i) {
                (void)environment_->evaluateFitness(organisms[i]);
            }
            // Apply selection and reproduction
            // This would be implemented based on the environment's capabilities
            stats_.total_generations++;
            stats_.last_generation_time = Clock::now();
            emitEvent({EventType::GENERATION_COMPLETED, stats_.total_generations, Clock::now(), "Generation completed", 0.0, 0});
            updateStats();
            performPeriodicTasks(stats_.total_generations);
        }
        return true;
    } catch (const std::exception& e) {
        emitEvent({EventType::ERROR_OCCURRED, stats_.total_generations, Clock::now(), "Error in generation: " + std::string(e.what()), 0.0, 0});
        return false;
    }
}

EvolutionEngine::EngineStats EvolutionEngine::getStats() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    
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
        auto organisms = environment_->getPopulation();
        stats.current_population = static_cast<uint32_t>(organisms.size());
        
        if (!organisms.empty()) {
            double total_fitness = 0.0;
            double best_fitness = 0.0;
            
            for (const auto& organism : organisms) {
                double fitness = environment_->evaluateFitness(organism.second);
                total_fitness += fitness;
                best_fitness = std::max(best_fitness, fitness);
            }
            
            stats.current_best_fitness = best_fitness;
            stats.current_avg_fitness = total_fitness / static_cast<double>(organisms.size());
        }
    }
    
    return stats;
}

void EvolutionEngine::registerEventCallback(EventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = callback;
}

void EvolutionEngine::unregisterEventCallback() {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = nullptr;
}

bool EvolutionEngine::saveState(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::string actual_filename = filename.empty() ? generateFilename("state", "json") : filename;
        
        if (!ensureSaveDirectory()) {
            return false;
        }
        
        // Simple state saving - would be more comprehensive in real implementation
        std::ofstream file(actual_filename);
        if (!file.is_open()) {
            return false;
        }
        
        file << "{\n";
        file << "  \"generations\": " << stats_.total_generations << ",\n";
        file << "  \"runtime_ms\": " << stats_.total_runtime_ms << ",\n";
        file << "  \"timestamp\": \"" << std::chrono::duration_cast<std::chrono::seconds>(Clock::now().time_since_epoch()).count() << "\"\n";
        file << "}\n";
        
        emitEvent({EventType::STATE_SAVED, stats_.total_generations, Clock::now(), "State saved to " + actual_filename, 0.0, 0});
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool EvolutionEngine::loadState(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        // Simple state loading - would be more comprehensive in real implementation
        std::string line;
        while (std::getline(file, line)) {
            // Parse state data
        }
        
        emitEvent({EventType::STATE_LOADED, stats_.total_generations, Clock::now(), "State loaded from " + filename, 0.0, 0});
        
        return true;
    } catch (const std::exception& e) {
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

std::vector<EvolutionEngine::Event> EvolutionEngine::getHistory() const {
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
        if (!file.is_open()) {
            return false;
        }
        
        auto stats = getStats();
        
        file << "Evolution Data Export\n";
        file << "=====================\n\n";
        file << "Total Generations: " << stats.total_generations << "\n";
        file << "Total Runtime (ms): " << stats.total_runtime_ms << "\n";
        file << "Generations per Second: " << stats.generations_per_second << "\n";
        file << "Current Population: " << stats.current_population << "\n";
        file << "Current Best Fitness: " << stats.current_best_fitness << "\n";
        file << "Current Average Fitness: " << stats.current_avg_fitness << "\n";
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

void EvolutionEngine::evolutionLoop() {
    int loop_count = 0;
    (void)loop_count;
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
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
        ++loop_count;
    }
    // Set stats_.is_running to false before exiting thread
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.is_running = false;
        stats_.is_paused = false;
    }
}

void EvolutionEngine::emitEvent(const Event& event) {
    if (event_callback_) {
        event_callback_(event);
    }
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.push_back(event);
    
    // Keep history size manageable
    if (history_.size() > 1000) {
        history_.erase(history_.begin(), history_.begin() + 100);
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
        saveState();
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
    std::string filename = generateFilename("backup_" + std::to_string(generation), "json");
    saveState(filename);
}

void EvolutionEngine::collectMetrics() {
    // TODO: Implement metrics collection
}

std::string EvolutionEngine::generateFilename(const std::string& prefix, const std::string& extension) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << config_.save_directory << "/" << prefix << "_";
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
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
        return false;
    }
}

} // namespace evosim 