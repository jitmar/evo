#include "gtest/gtest.h"
#include "core/evolution_engine.h"
#include "core/environment.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <functional>
#include <filesystem>

// A helper function to wait until a condition is met, with a timeout.
// This is much more reliable for testing threaded code than fixed sleeps.
bool waitFor(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
    auto start = std::chrono::steady_clock::now();
    auto last_log = start;
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (condition()) {
            return true;
        }
        
        // Log progress every second
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() >= 1) {
            spdlog::debug("Still waiting for condition... Time elapsed: {} ms", 
                std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
            last_log = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

// Basic fixture for initialization and simple non-state tests
class EvolutionEngineBasicTest : public ::testing::Test {
protected:
    std::unique_ptr<evosim::EvolutionEngine> engine;
    std::shared_ptr<evosim::Environment> test_env;
    evosim::Environment::Config test_env_config;
    evosim::EvolutionEngine::EventType last_event_type_;
    std::chrono::steady_clock::time_point last_event_time_;
    uint64_t generation_count_ = 0;

    static void SetUpTestSuite() {
        // Configure logging for better test diagnostics
        spdlog::set_level(spdlog::level::trace);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] [%!] %v");
        spdlog::info("Setting up EvolutionEngineBasicTest test suite");
    }

    void SetUp() override {
        // Basic environment config - use more realistic settings
        test_env_config.generation_time_ms = 50;    // Give enough time for generation to complete
        test_env_config.initial_population_size = 3; // Minimal population for fastest processing
        
        // Set up event tracking
        last_event_type_ = evosim::EvolutionEngine::EventType::ENGINE_STOPPED;
        last_event_time_ = std::chrono::steady_clock::now();
        
        // Configure environment settings for testing
        test_env_config.initial_bytecode_size = 16;      // Very small initial programs
        test_env_config.max_mutations = 2;               // Fewer mutations
        test_env_config.mutation_rate = 0.005;          // Lower mutation rate
        
        // Disable complex features for testing
        test_env_config.enable_aging = false;
        test_env_config.enable_competition = false;
        test_env_config.enable_cooperation = false;
        test_env_config.enable_predation = false;
        test_env_config.enable_random_catastrophes = false;
        
        // Configure VM for testing with minimal resources
        evosim::BytecodeVM::Config vm_config;
        vm_config.image_width = 64;       // Smaller image size for faster processing
        vm_config.image_height = 64;
        vm_config.memory_size = 256;      // Smaller memory size
        vm_config.stack_size = 64;        // Smaller stack
        vm_config.return_stack_size = 16; // Smaller return stack
        vm_config.max_instructions = 1000; // Fewer max instructions
        
        // Create environment with test configurations
        test_env = std::make_shared<evosim::Environment>(test_env_config, vm_config);
        if (!test_env) {
            throw std::runtime_error("Failed to create test environment");
        }

        // Create engine with basic config - no state management
        evosim::EvolutionEngine::Config engine_config;
        engine_config.enable_save_state = false;  // Disable state management
        engine_config.enable_backup = false;
        engine = std::make_unique<evosim::EvolutionEngine>(test_env, engine_config);
        if (!engine) {
            throw std::runtime_error("Failed to create evolution engine");
        }
    }

    void TearDown() override {
        if (engine) {
            engine->stop();
        }
    }
};

// Fixture for tests that need state management
class EvolutionEngineStateTest : public ::testing::Test {
protected:
    std::unique_ptr<evosim::EvolutionEngine> engine;
    std::shared_ptr<evosim::Environment> test_env;
    evosim::Environment::Config test_env_config;
    std::filesystem::path test_dir;
    std::filesystem::path test_state_file;

    void SetUp() override {
        // Create a unique temporary test directory
        test_dir = std::filesystem::temp_directory_path() / "evosim_test" / std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir);
        test_state_file = test_dir / "test_engine_state.evo";

        // Basic configuration for testing
        test_env_config.generation_time_ms = 20;
        test_env_config.initial_population_size = 10;
        
        // Configure VM settings for testing
        test_env_config.initial_bytecode_size = 32;      // Smaller initial programs
        test_env_config.max_mutations = 2;               // Fewer mutations
        test_env_config.mutation_rate = 0.005;          // Lower mutation rate
        
        // Disable complex features for testing
        test_env_config.enable_aging = false;
        test_env_config.enable_competition = false;
        test_env_config.enable_cooperation = false;
        test_env_config.enable_predation = false;
        test_env_config.enable_random_catastrophes = false;
        
        // Create environment
        test_env = std::make_shared<evosim::Environment>(test_env_config);
        if (!test_env) {
            throw std::runtime_error("Failed to create test environment");
        }

        // Create engine with state management configured to use test directory
        evosim::EvolutionEngine::Config engine_config;
        engine_config.save_directory = test_dir.string();
        engine = std::make_unique<evosim::EvolutionEngine>(test_env, engine_config);
        if (!engine) {
            throw std::runtime_error("Failed to create evolution engine");
        }
    }

    void TearDown() override {
        if (engine) {
            engine->stop();
        }
        // Clean up the entire test directory
        std::filesystem::remove_all(test_dir);
    }
};

TEST_F(EvolutionEngineBasicTest, Initialization) {
    auto stats = engine->getStats();
    EXPECT_EQ(stats.current_population, test_env_config.initial_population_size);
    EXPECT_EQ(stats.total_generations, 0);
}

// Separate fixture for threading-specific tests to ensure isolation
class EvolutionEngineThreadTest : public ::testing::Test {
protected:
    std::unique_ptr<evosim::EvolutionEngine> engine;
    std::shared_ptr<evosim::Environment> test_env;
    evosim::Environment::Config test_env_config;

    void SetUp() override {
        // Basic configuration for testing
        test_env_config.generation_time_ms = 20;
        test_env_config.initial_population_size = 10;
        
        // Configure VM settings for testing
        test_env_config.initial_bytecode_size = 32;      // Smaller initial programs
        test_env_config.max_mutations = 2;               // Fewer mutations
        test_env_config.mutation_rate = 0.005;          // Lower mutation rate
        
        // Disable complex features for testing
        test_env_config.enable_aging = false;
        test_env_config.enable_competition = false;
        test_env_config.enable_cooperation = false;
        test_env_config.enable_predation = false;
        test_env_config.enable_random_catastrophes = false;
        
        test_env = std::make_shared<evosim::Environment>(test_env_config);
        
        // Create engine with state management disabled
        evosim::EvolutionEngine::Config engine_config;
        engine_config.enable_save_state = false;
        engine_config.enable_backup = false;
        engine = std::make_unique<evosim::EvolutionEngine>(test_env, engine_config);
    }

    void TearDown() override {
        if (engine) {
            engine->stop();
        }
    }
};

TEST_F(EvolutionEngineThreadTest, RunGenerationsThreaded) {
    engine->start();

    // Wait for at least 2 generations to pass using our reliable helper.
    ASSERT_TRUE(waitFor([this]() {
        return engine->getStats().total_generations >= 2;
    })) << "Engine did not complete enough generations in time.";

    auto stats = engine->getStats();
    EXPECT_GT(stats.total_generations, 0);
    EXPECT_GT(stats.current_population, 0);
}

TEST_F(EvolutionEngineBasicTest, PauseAndResume) {
    spdlog::info("Starting PauseAndResume test");
    
    // Set up event tracking
    engine->registerEventCallback([this](const evosim::EvolutionEngine::Event& event) {
        last_event_type_ = event.type;
        last_event_time_ = std::chrono::steady_clock::now();
        if (event.type == evosim::EvolutionEngine::EventType::GENERATION_COMPLETED) {
            generation_count_++;
            spdlog::info("Generation {} completed at t={} ms", 
                event.generation,
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    event.timestamp - last_event_time_).count());
        }
        if (event.type == evosim::EvolutionEngine::EventType::ENGINE_PAUSED || event.type == evosim::EvolutionEngine::EventType::ENGINE_RESUMED) {
            spdlog::info("State change event received: {}", static_cast<int>(event.type));
        }
        spdlog::info("Engine event: {} at t={} ms", 
            static_cast<int>(event.type),
            std::chrono::duration_cast<std::chrono::milliseconds>(
                event.timestamp - last_event_time_).count());
    });
    
    // Log initial state and config
    auto initial_stats = engine->getStats();
    spdlog::info("Initial state - generations: {}, population: {}, running: {}, paused: {}", 
        initial_stats.total_generations, initial_stats.current_population,
        initial_stats.is_running, initial_stats.is_paused);
        
    spdlog::info("Environment config - generation_time: {} ms, population: {}", 
        test_env_config.generation_time_ms, test_env_config.initial_population_size);

    // Verify environment is properly initialized
    if (auto env = engine->getEnvironment()) {
        auto env_stats = env->getStats();
        spdlog::info("Environment state - generation: {}, population: {}", 
            env_stats.generation, env_stats.population_size);
        ASSERT_GT(env_stats.population_size, 0) << "Environment not properly initialized with organisms";
    }
    
    // Start the engine
    ASSERT_TRUE(engine->start()) << "Failed to start engine";
    spdlog::info("Engine started - waiting for startup event");
    
    // Wait for ENGINE_STARTED event
    ASSERT_TRUE(waitFor([this]() {
        return last_event_type_ == evosim::EvolutionEngine::EventType::ENGINE_STARTED;
    })) << "Engine did not emit startup event";
    
    // Verify engine started correctly
    auto post_start_stats = engine->getStats();
    spdlog::info("Post-start state - running: {}, paused: {}", 
        post_start_stats.is_running, post_start_stats.is_paused);
    ASSERT_TRUE(post_start_stats.is_running) << "Engine not marked as running";

    // Wait for at least one generation to pass to have a baseline.
    ASSERT_TRUE(waitFor([this]() {
        auto stats = engine->getStats();
        spdlog::debug("Waiting for generation - current: {}, population: {}, running: {}, paused: {}", 
            stats.total_generations, stats.current_population, stats.is_running, stats.is_paused);
        return stats.total_generations > 0;
    })) << "Engine did not complete a generation in time.";

    spdlog::info("First generation completed, pausing engine...");
    engine->pause();

    // Wait for the engine to confirm it has paused via an event.
    // This is more reliable than assuming the state changed instantly or using sleep.
    // NOTE: This assumes your EventType enum has ENGINE_PAUSED.
    // If not, this is a highly recommended addition for robust testing of async systems.
    ASSERT_TRUE(waitFor([this]() {
        return last_event_type_ == evosim::EvolutionEngine::EventType::ENGINE_PAUSED;
    }, std::chrono::seconds(2))) << "Engine did not emit a PAUSED event in time.";
    EXPECT_TRUE(engine->isPaused()) << "Engine state should be paused after PAUSED event.";

    // After pausing, capture the generation count.
    auto generations_when_paused = engine->getStats().total_generations;
    spdlog::info("Engine confirmed paused at generation {}", generations_when_paused);

    // Wait for a few generation cycles to ensure the engine is truly paused.
    std::this_thread::sleep_for(std::chrono::milliseconds(test_env_config.generation_time_ms * 5));

    // The number of generations should not have changed.
    EXPECT_EQ(engine->getStats().total_generations, generations_when_paused);
    
    spdlog::info("Resuming engine...");
    engine->resume();
    EXPECT_FALSE(engine->isPaused());

    // Wait for the engine to confirm it has resumed.
    ASSERT_TRUE(waitFor([this]() {
        // NOTE: This assumes an ENGINE_RESUMED event exists.
        return last_event_type_ == evosim::EvolutionEngine::EventType::ENGINE_RESUMED;
    }, std::chrono::seconds(2))) << "Engine did not emit a RESUMED event in time.";

    // Wait for the generation count to increase.
    ASSERT_TRUE(waitFor([this, generations_when_paused]() {
        return engine->getStats().total_generations > generations_when_paused;
    })) << "Engine did not resume and complete a new generation.";
}

TEST_F(EvolutionEngineStateTest, SaveAndLoadState) {
    engine->start();

    // Wait for a few generations to pass so we have interesting state to save.
    ASSERT_TRUE(waitFor([this]() {
        return engine->getStats().total_generations >= 2;
    })) << "Engine did not complete enough generations in time.";

    engine->pause(); // Pause to ensure a consistent state for saving.
    auto stats_before_save = engine->getStats();
    EXPECT_GT(stats_before_save.total_generations, 0);

    ASSERT_TRUE(engine->saveState(test_state_file));

    // Create a new engine to load the state into.
    auto new_engine = std::make_unique<evosim::EvolutionEngine>(std::make_shared<evosim::Environment>());
    ASSERT_TRUE(new_engine->loadState(test_state_file));
    auto stats_after_load = new_engine->getStats();

    EXPECT_EQ(stats_after_load.total_generations, stats_before_save.total_generations);
    EXPECT_EQ(stats_after_load.current_population, stats_before_save.current_population);
}