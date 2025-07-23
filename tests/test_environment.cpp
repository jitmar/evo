#include "test_common.h"
#include "core/environment.h"
#include "core/organism.h"

namespace evosim {

class EnvironmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        Environment::Config config;
        config.max_population = 20;
        config.initial_population = 0;
        config.min_population = 1;
        config.mutation_rate = 0.01;
        config.max_mutations = 3;
        config.selection_pressure = 0.7;
        config.resource_abundance = 1.0;
        config.generation_time_ms = 1000;
        config.enable_aging = true;
        config.max_age_ms = 30000;
        config.enable_competition = true;
        config.competition_intensity = 0.5;
        config.enable_cooperation = false;
        config.cooperation_bonus = 0.1;
        environment_ = std::make_unique<Environment>(config);
    }
    
    void TearDown() override {
        environment_.reset();
    }
    
    std::unique_ptr<Environment> environment_;
    
    std::shared_ptr<Organism> createTestOrganism() {
        Organism::Bytecode bytecode = {0x01, 0x42, 0x0B}; // PUSH 0x42, HALT
        return std::make_shared<Organism>(bytecode, 0);
    }
};

TEST_F(EnvironmentTest, Constructor) {
    EXPECT_NE(environment_, nullptr);
    EXPECT_EQ(environment_->getPopulation().size(), 0);
    auto stats = environment_->getStats();
    EXPECT_EQ(stats.population_size, 0);
}

TEST_F(EnvironmentTest, AddOrganism) {
    auto organism = createTestOrganism();
    EXPECT_TRUE(environment_->addOrganism(organism));
    EXPECT_EQ(environment_->getPopulation().size(), 1);
}

TEST_F(EnvironmentTest, AddNullOrganism) {
    EXPECT_FALSE(environment_->addOrganism(nullptr));
    EXPECT_EQ(environment_->getPopulation().size(), 0);
}

TEST_F(EnvironmentTest, RemoveOrganism) {
    auto organism = createTestOrganism();
    environment_->addOrganism(organism);
    uint64_t organism_id = organism->getStats().id;
    EXPECT_TRUE(environment_->removeOrganism(organism_id));
    EXPECT_EQ(environment_->getPopulation().size(), 0);
}

TEST_F(EnvironmentTest, RemoveNonexistentOrganism) {
    EXPECT_FALSE(environment_->removeOrganism(999));
}

TEST_F(EnvironmentTest, GetOrganisms) {
    auto organism1 = createTestOrganism();
    auto organism2 = createTestOrganism();
    environment_->addOrganism(organism1);
    environment_->addOrganism(organism2);
    auto organisms = environment_->getPopulation();
    EXPECT_EQ(organisms.size(), 2);
}

TEST_F(EnvironmentTest, UpdateEnvironment) {
    // Tune config to encourage reproduction
    Environment::Config new_config = environment_->getConfig();
    new_config.min_population = 5;
    new_config.max_population = 20;
    new_config.mutation_rate = 0.1;
    new_config.selection_pressure = 0.2;
    environment_->setConfig(new_config);
    // Add multiple organisms to ensure meaningful test
    for (int i = 0; i < 5; ++i) {
        environment_->addOrganism(createTestOrganism());
    }
    auto stats_before = environment_->getStats();
    EXPECT_TRUE(environment_->update());
    auto stats_after = environment_->getStats();
    EXPECT_GE(stats_after.generation, stats_before.generation + 1);
    // Optionally, check that population is still nonzero
    EXPECT_GT(environment_->getPopulation().size(), 0u);
}

TEST_F(EnvironmentTest, Clear) {
    auto organism = createTestOrganism();
    environment_->addOrganism(organism);
    environment_->clear();
    EXPECT_EQ(environment_->getPopulation().size(), 0u);
    auto stats = environment_->getStats();
    EXPECT_EQ(stats.population_size, 0u);
}

TEST_F(EnvironmentTest, GetStats) {
    auto organism = createTestOrganism();
    environment_->addOrganism(organism);
    auto stats = environment_->getStats();
    EXPECT_EQ(stats.population_size, 1);
    EXPECT_GE(stats.avg_fitness, 0.0);
    EXPECT_LE(stats.avg_fitness, 1.0);
}

TEST_F(EnvironmentTest, SetConfig) {
    Environment::Config new_config;
    new_config.max_population = 30;
    new_config.initial_population = 5;
    new_config.min_population = 2;
    new_config.mutation_rate = 0.02;
    new_config.max_mutations = 4;
    new_config.selection_pressure = 0.8;
    new_config.resource_abundance = 1.5;
    new_config.generation_time_ms = 2000;
    new_config.enable_aging = false;
    new_config.max_age_ms = 60000;
    new_config.enable_competition = false;
    new_config.competition_intensity = 0.3;
    new_config.enable_cooperation = true;
    new_config.cooperation_bonus = 0.2;

    environment_->setConfig(new_config);

    const auto& config = environment_->getConfig();
    EXPECT_EQ(config.max_population, 30u);
    EXPECT_EQ(config.mutation_rate, 0.02);
    EXPECT_EQ(config.enable_cooperation, true);
}

TEST_F(EnvironmentTest, ApplyEnvironmentalPressures) {
    // Setup: Add several organisms
    for (int i = 0; i < 20; ++i) {
        environment_->addOrganism(createTestOrganism());
    }
    size_t before = environment_->getPopulation().size();
    environment_->applyEnvironmentalPressures();
    size_t after = environment_->getPopulation().size();
    // The population should not increase after applying pressures
    EXPECT_LE(after, before);
    // The function should not crash or throw
}

TEST_F(EnvironmentTest, EvaluateFitness) {
    auto organism = createTestOrganism();
    environment_->addOrganism(organism);
    double fitness = environment_->evaluateFitness(organism);
    EXPECT_GE(fitness, 0.0);
    EXPECT_LE(fitness, 1.0);
}

TEST_F(EnvironmentTest, SelectForReproduction) {
    auto organism1 = createTestOrganism();
    auto organism2 = createTestOrganism();
    environment_->addOrganism(organism1);
    environment_->addOrganism(organism2);
    auto selected = environment_->selectForReproduction(1);
    EXPECT_EQ(selected.size(), 1u);
    EXPECT_TRUE(selected[0] != nullptr);
}

} // namespace evosim 