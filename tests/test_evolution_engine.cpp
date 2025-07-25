#include "gtest/gtest.h"
#include "core/evolution_engine.h"
#include "core/environment.h"
#include <thread>
#include <chrono>
#include <functional>
#include <filesystem>

// A helper function to wait until a condition is met, with a timeout.
// This is much more reliable for testing threaded code than fixed sleeps.
bool waitFor(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

class EvolutionEngineTest : public ::testing::Test {
protected:
    std::unique_ptr<evosim::EvolutionEngine> engine;
    std::shared_ptr<evosim::Environment> test_env;
    evosim::Environment::Config test_env_config;
    const std::string test_state_file = "test_engine_state.evo";

    void SetUp() override {
        // Use a very short generation time for tests to make them fast and reliable.
        test_env_config.generation_time_ms = 20;
        test_env_config.initial_population = 10;
        test_env_config.initial_bytecode_size = 32;

        test_env = std::make_shared<evosim::Environment>(test_env_config);
        engine = std::make_unique<evosim::EvolutionEngine>(test_env);

        // Clean up any old state files before a test runs.
        std::filesystem::remove(test_state_file);
    }

    void TearDown() override {
        if (engine) {
            engine->stop();
        }
        // Clean up state files after a test runs.
        std::filesystem::remove(test_state_file);
    }
};

TEST_F(EvolutionEngineTest, Initialization) {
    test_env->initialize(test_env_config.initial_bytecode_size);
    auto stats = engine->getStats();
    EXPECT_EQ(stats.current_population, test_env_config.initial_population);
    EXPECT_EQ(stats.total_generations, 0);
}

TEST_F(EvolutionEngineTest, RunGenerationsThreaded) {
    test_env->initialize(test_env_config.initial_bytecode_size);
    engine->start();

    // Wait for at least 2 generations to pass using our reliable helper.
    ASSERT_TRUE(waitFor([this]() {
        return engine->getStats().total_generations >= 2;
    })) << "Engine did not complete enough generations in time.";

    auto stats = engine->getStats();
    EXPECT_GT(stats.total_generations, 0);
    EXPECT_GT(stats.current_population, 0);
}

TEST_F(EvolutionEngineTest, PauseAndResume) {
    test_env->initialize(test_env_config.initial_bytecode_size);
    engine->start();

    // Wait for at least one generation to pass to have a baseline.
    ASSERT_TRUE(waitFor([this]() {
        return engine->getStats().total_generations > 0;
    })) << "Engine did not complete a generation in time.";

    engine->pause();
    EXPECT_TRUE(engine->isPaused());

    // After pausing, capture the generation count.
    auto generations_when_paused = engine->getStats().total_generations;

    // Wait for a few generation cycles to ensure the engine is truly paused.
    std::this_thread::sleep_for(std::chrono::milliseconds(test_env_config.generation_time_ms * 5));

    // The number of generations should not have changed.
    EXPECT_EQ(engine->getStats().total_generations, generations_when_paused);

    engine->resume();
    EXPECT_FALSE(engine->isPaused());

    // Wait for the generation count to increase.
    ASSERT_TRUE(waitFor([this, generations_when_paused]() {
        return engine->getStats().total_generations > generations_when_paused;
    })) << "Engine did not resume and complete a new generation.";
}

TEST_F(EvolutionEngineTest, SaveAndLoadState) {
    test_env->initialize(test_env_config.initial_bytecode_size);
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