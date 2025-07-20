#include <gtest/gtest.h>
#include "core/evolution_engine.h"
#include "core/environment.h"

namespace evosim {

class EvolutionEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        Environment::Config env_config;
        env_config.initial_population = 10;
        env_config.max_population = 20;
        
        auto environment = std::make_shared<Environment>(env_config);
        
        EvolutionEngine::Config config;
        config.auto_start = false;
        config.enable_logging = true;
        
        engine_ = std::make_unique<EvolutionEngine>(environment, config);
    }
    
    void TearDown() override {
        engine_.reset();
    }
    
    std::unique_ptr<EvolutionEngine> engine_;
};

TEST_F(EvolutionEngineTest, Constructor) {
    EXPECT_NE(engine_, nullptr);
}

TEST_F(EvolutionEngineTest, StartStop) {
    // Test that we can get stats before starting
    auto stats = engine_->getStats();
    EXPECT_FALSE(stats.is_running);
    
    // Test start
    EXPECT_TRUE(engine_->start());
    stats = engine_->getStats();
    EXPECT_TRUE(stats.is_running);
    
    // Test stop
    engine_->stop();
    stats = engine_->getStats();
    EXPECT_FALSE(stats.is_running);
}

TEST_F(EvolutionEngineTest, GetStats) {
    auto stats = engine_->getStats();
    EXPECT_FALSE(stats.is_running);
    EXPECT_EQ(stats.total_generations, 0);
    EXPECT_EQ(stats.current_population, 10); // Updated: matches environment's initial_population
    
    engine_->start();
    stats = engine_->getStats();
    EXPECT_TRUE(stats.is_running);
    EXPECT_GT(stats.current_population, 0);
}

TEST_F(EvolutionEngineTest, SetConfig) {
    EvolutionEngine::Config new_config;
    new_config.auto_start = true;
    new_config.enable_logging = false;
    new_config.enable_save_state = false;
    
    engine_->setConfig(new_config);
    
    // Test that config was applied
    auto config = engine_->getConfig();
    EXPECT_EQ(config.auto_start, true);
    EXPECT_EQ(config.enable_logging, false);
    EXPECT_EQ(config.enable_save_state, false);
}

TEST_F(EvolutionEngineTest, GetEnvironment) {
    auto environment = engine_->getEnvironment();
    EXPECT_NE(environment, nullptr);
}

// Manual mode: runGeneration() only, do not call start()
// TEST_F(EvolutionEngineTest, RunGenerations) {
//     // Run for a few generations manually
//     for (int i = 0; i < 5; ++i) {
//         EXPECT_TRUE(engine_->runGeneration());
//     }
//     auto stats = engine_->getStats();
//     EXPECT_GE(stats.total_generations, 5);
// }

// Threaded mode: start() and let the engine run generations automatically
TEST_F(EvolutionEngineTest, RunGenerationsThreaded) {
    EXPECT_TRUE(engine_->start());
    // Wait for a few generations to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    engine_->stop();
    auto stats = engine_->getStats();
    EXPECT_GT(stats.total_generations, 0);
}

TEST_F(EvolutionEngineTest, MultipleStartStop) {
    // Should be able to start and stop multiple times
    EXPECT_TRUE(engine_->start());
    engine_->stop();
    
    EXPECT_TRUE(engine_->start());
    engine_->stop();
    
    EXPECT_TRUE(engine_->start());
    engine_->stop();
}

} // namespace evosim 