#pragma once

#include "environment.h"
#include <thread>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <deque>

namespace evosim {

/**
 * @brief Main evolution engine that orchestrates the simulation
 * 
 * The evolution engine manages the environment, controls the evolution
 * process, and provides interfaces for monitoring and control.
 */
class EvolutionEngine {
public:
    using EnvironmentPtr = std::shared_ptr<Environment>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    /**
     * @brief Evolution engine statistics
     */
    struct EngineStats {
        uint64_t total_generations = 0;     ///< Total generations completed
        uint64_t total_runtime_ms = 0;      ///< Total runtime in milliseconds
        double generations_per_second = 0.0;  ///< Evolution rate
        bool is_running = false;                ///< Engine running state
        bool is_paused = false;                 ///< Engine paused state
        TimePoint start_time = TimePoint();           ///< Engine start time
        TimePoint last_generation_time = TimePoint(); ///< Last generation completion time
        uint32_t current_population = 0;    ///< Current population size
        double current_best_fitness = 0.0;    ///< Current best fitness score
        double current_avg_fitness = 0.0;     ///< Current average fitness score
        EngineStats() = default;
    };

    /**
     * @brief Engine configuration
     */
    struct Config {
        bool auto_start;            ///< Auto-start evolution
        bool enable_logging;        ///< Enable detailed logging
        bool enable_save_state;     ///< Enable state saving
        uint32_t save_interval_generations; ///< Save state every N generations
        std::string save_directory; ///< Directory for save files
        bool enable_visualization;  ///< Enable real-time visualization
        uint32_t visualization_interval; ///< Visualization update interval
        bool enable_metrics;        ///< Enable performance metrics
        uint32_t metrics_interval;  ///< Metrics collection interval
        bool enable_backup;         ///< Enable automatic backups
        uint32_t backup_interval;   ///< Backup interval in generations
        uint64_t max_generations;   ///< Stop after N generations (0 for infinite)
        
        Config() : auto_start(false), enable_logging(true), enable_save_state(true),
                   save_interval_generations(100), save_directory("saves"),
                   enable_visualization(false), visualization_interval(10),
                   enable_metrics(true), metrics_interval(50), enable_backup(true),
                   backup_interval(1000), max_generations(0) {}
    };

    /**
     * @brief Evolution event types
     */
    enum class EventType {
        GENERATION_COMPLETED,    ///< Generation completed
        ORGANISM_BORN,          ///< New organism created
        ORGANISM_DIED,          ///< Organism died
        FITNESS_IMPROVED,       ///< Best fitness improved
        POPULATION_CHANGED,     ///< Population size changed
        ENGINE_STARTED,         ///< Engine started
        ENGINE_STOPPED,         ///< Engine stopped
        ENGINE_PAUSED,          ///< Engine paused
        ENGINE_RESUMED,         ///< Engine resumed
        STATE_SAVED,            ///< State saved
        STATE_LOADED,           ///< State loaded
        ERROR_OCCURRED          ///< Error occurred
    };

    /**
     * @brief Evolution event
     */
    struct Event {
        EventType type;         ///< Event type
        uint64_t generation;    ///< Generation number
        TimePoint timestamp;    ///< Event timestamp
        std::string message;    ///< Event message
        double fitness_score;   ///< Associated fitness score (if applicable)
        uint64_t organism_id;   ///< Associated organism ID (if applicable)
    };

    /**
     * @brief Event callback function type
     */
    using EventCallback = std::function<void(const Event&)>;

    /**
     * @brief Constructor
     * @param environment Environment to manage
     * @param config Engine configuration
     */
    explicit EvolutionEngine(EnvironmentPtr environment, const Config& config = Config());

    /**
     * @brief Destructor
     */
    ~EvolutionEngine();

    /**
     * @brief Start evolution process
     * @return True if started successfully
     */
    bool start();

    /**
     * @brief Stop evolution process
     * @return True if stopped successfully
     */
    bool stop();

    /**
     * @brief Pause evolution process
     * @return True if paused successfully
     */
    bool pause();

    /**
     * @brief Resume evolution process
     * @return True if resumed successfully
     */
    bool resume();

    /**
     * @brief Get engine statistics
     * @return Current engine statistics
     */
    EngineStats getStats() const;

    /**
     * @brief Get engine configuration
     * @return Current configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * @brief Set engine configuration
     * @param config New configuration
     */
    void setConfig(const Config& config) { config_ = config; }

    /**
     * @brief Register event callback
     * @param callback Event callback function
     */
    void registerEventCallback(EventCallback callback);

    /**
     * @brief Unregister event callback
     */
    void unregisterEventCallback();

    /**
     * @brief Save current state
     * @param filename Optional filename (auto-generated if empty)
     * @return True if successful
     */
    bool saveState(const std::string& filename = "");

    /**
     * @brief Load state from file
     * @param filename State file to load
     * @return True if successful
     */
    bool loadState(const std::string& filename);

    /**
     * @brief Get environment
     * @return Environment pointer
     */
    EnvironmentPtr getEnvironment() const { return environment_; }

    /**
     * @brief Check if engine is running
     * @return True if running
     */
    bool isRunning() const { return running_; }

    /**
     * @brief Check if engine is paused
     * @return True if paused
     */
    bool isPaused() const { return paused_; }

    /**
     * @brief Wait for evolution to complete
     * @param timeout_ms Timeout in milliseconds (0 for infinite)
     * @return True if completed within timeout
     */
    bool waitForCompletion(uint32_t timeout_ms = 0);

    /**
     * @brief Get evolution history
     * @return Vector of recent events
     */
    std::deque<Event> getHistory() const;

    /**
     * @brief Clear evolution history
     */
    void clearHistory();

    /**
     * @brief Export evolution data
     * @param filename Output filename
     * @return True if successful
     */
    bool exportData(const std::string& filename) const;

    /**
     * @brief Generates and saves a test phenotype image filled with random pixels.
     * @param width The width of the image to generate.
     * @param height The height of the image to generate.
     * @return The path to the saved file, or an empty string on failure.
     */
    std::string generateTestPhenotype(uint32_t width, uint32_t height);

private:
    EnvironmentPtr environment_;        ///< Managed environment
    Config config_;                     ///< Engine configuration
    EngineStats stats_;                 ///< Engine statistics
    std::atomic<bool> running_;         ///< Engine running state
    std::atomic<bool> paused_;          ///< Engine paused state
    std::atomic<bool> should_stop_;     ///< Stop request flag
    std::thread evolution_thread_;      ///< Evolution thread
    // TODO: Consider refactoring the locking strategy to use a standard std::mutex
    // instead of a recursive_mutex. This would require careful analysis to ensure
    // no deadlocks occur (e.g., by using private non-locking helper methods).
    mutable std::recursive_mutex mutex_;///< Recursive mutex for thread safety
    std::condition_variable_any cv_;    ///< Condition variable for synchronization (works with recursive_mutex)
    EventCallback event_callback_;      ///< Event callback function
    std::deque<Event> history_;         ///< Evolution history
    mutable std::mutex history_mutex_;  ///< History mutex

    /**
     * @brief Logs the effective configuration of the engine and its components.
     */
    void logEffectiveConfig() const;

    /**
     * @brief Main evolution loop
     */
    void evolutionLoop();

    /**
     * @brief Emit evolution event
     * @param event Event to emit
     */
    void emitEvent(const Event& event);

    /**
     * @brief Update engine statistics
     */
    void updateStats();

    /**
     * @brief Perform periodic tasks
     * @param generation Current generation
     */
    void performPeriodicTasks(uint64_t generation);

    /**
     * @brief Save automatic backup
     * @param generation Current generation
     */
    void saveBackup(uint64_t generation);

    /**
     * @brief Save state to file
     * @param filename Optional filename (auto-generated if empty)
     * @return True if successful
     */
    bool saveState_unlocked(const std::string& filename = "");

    /**
     * @brief Collect performance metrics
     */
    void collectMetrics();

    /**
     * @brief Generate automatic filename
     * @param prefix Filename prefix
     * @param extension File extension
     * @return Generated filename
     */
    std::string generateFilename(const std::string& prefix, const std::string& extension) const;

    /**
     * @brief Ensure save directory exists
     * @return True if directory exists or created
     */
    bool ensureSaveDirectory() const;

    /**
     * @brief Run single generation
     * @return True if generation completed successfully
     */
    bool run_generation_();
};

} // namespace evosim 