#include "test_common.h"
#include "core/environment.h"
#include "core/organism.h"
#include <filesystem>

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
        // This bytecode just pushes a value and halts, producing a blank image.
        Organism::Bytecode bytecode = {0x01, 0x42, 0xFF}; // PUSH 0x42, HALT
        BytecodeVM vm; // Create a default VM for test organism creation
        return std::make_shared<Organism>(bytecode, vm, 0);
    }

    std::shared_ptr<Organism> createTestOrganismWithFitness(double fitness) {
        auto org = createTestOrganism();
        org->setFitnessScore(fitness);
        return org;
    }
};

TEST_F(EnvironmentTest, Constructor) {
    EXPECT_NE(environment_, nullptr);
    EXPECT_EQ(environment_->getPopulation().size(), 0);
    auto stats = environment_->getStats();
    EXPECT_EQ(stats.population_size, 0);
}

TEST_F(EnvironmentTest, ConstructorWithCustomSubConfigs) {
    // 1. Define custom configurations for sub-components.
    BytecodeVM::Config vm_config;
    vm_config.image_width = 64;
    vm_config.max_instructions = 5000;

    SymmetryAnalyzer::Config analyzer_config;
    analyzer_config.horizontal_weight = 0.99;
    analyzer_config.enable_vertical = false;

    // 2. Create the Environment with these specific configs.
    Environment custom_env({}, vm_config, analyzer_config);

    // 3. Verify that the internal components have the correct configuration.
    const auto internal_vm_config = custom_env.getVMConfig();
    EXPECT_EQ(internal_vm_config.image_width, 64);
    EXPECT_EQ(internal_vm_config.max_instructions, 5000);

    const auto internal_analyzer_config = custom_env.getAnalyzerConfig();
    EXPECT_DOUBLE_EQ(internal_analyzer_config.horizontal_weight, 0.99);
    EXPECT_FALSE(internal_analyzer_config.enable_vertical);
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
    new_config.enable_predation = false;
    new_config.enable_random_catastrophes = false;

    environment_->setConfig(new_config);

    const auto& config = environment_->getConfig();
    EXPECT_EQ(config.max_population, 30u);
    EXPECT_EQ(config.mutation_rate, 0.02);
    EXPECT_EQ(config.enable_cooperation, true);
    EXPECT_EQ(config.enable_predation, false);
    EXPECT_EQ(config.enable_random_catastrophes, false);
}

TEST_F(EnvironmentTest, ApplyEnvironmentalPressuresSelectsCorrectly) {
    // This test verifies that selection pressure correctly removes low-fitness organisms.

    // 1. Configure the environment for high selection pressure and disable other pressures.
    Environment::Config config = environment_->getConfig();
    config.selection_pressure = 0.5; // Target removing 50% of the population.
    config.enable_aging = false;
    config.enable_competition = false;
    config.enable_predation = false;
    config.enable_random_catastrophes = false;
    config.resource_abundance = 10.0; // Effectively infinite resources.
    environment_->setConfig(config);

    // 2. Create a predictable population: 5 high-fitness, 5 low-fitness.
    for (int i = 0; i < 5; ++i) {
        environment_->addOrganism(createTestOrganismWithFitness(0.9)); // High fitness
    }
    for (int i = 0; i < 5; ++i) {
        environment_->addOrganism(createTestOrganismWithFitness(0.1)); // Low fitness
    }
    ASSERT_EQ(environment_->getPopulation().size(), 10);

    // 3. Apply the pressures. This should trigger apply_selection_pressure_().
    environment_->applyEnvironmentalPressures();

    // 4. Verify the outcome: The 5 low-fitness organisms should have been removed.
    auto final_population = environment_->getPopulation();
    EXPECT_EQ(final_population.size(), 5);
    for (const auto& pair : final_population) {
        ASSERT_NE(pair.second, nullptr);
        EXPECT_NEAR(pair.second->getFitnessScore(), 0.9, 1e-6);
    }
}

TEST_F(EnvironmentTest, EvaluateFitness) {
    auto organism = createTestOrganism();
    environment_->addOrganism(organism);
    double fitness = environment_->evaluateFitness(organism);
    EXPECT_GE(fitness, 0.0);
    EXPECT_LE(fitness, 1.0);
}

TEST_F(EnvironmentTest, BlankOrganismHasZeroFitness) {
    // This organism's bytecode does not draw anything, resulting in a blank (black) image.
    // The `createTestOrganism` helper provides such an organism.
    auto blank_organism = createTestOrganism();

    // The fitness function should explicitly penalize blank/monochrome images
    // to prevent stagnation, assigning them a fitness of zero.
    double fitness = environment_->evaluateFitness(blank_organism);

    EXPECT_DOUBLE_EQ(fitness, 0.0);
}

TEST_F(EnvironmentTest, SaveAndLoadState) {
    const std::string save_file = "test_env_state.json";

    // Use a "calm" configuration for this test to make population size predictable.
    // Disable selection pressures that would cull the population.
    Environment::Config calm_config = environment_->getConfig();
    calm_config.selection_pressure = 0.0;
    calm_config.enable_competition = false;
    calm_config.enable_aging = false;
    calm_config.enable_predation = false;
    environment_->setConfig(calm_config);

    // 1. Add some organisms to create a state
    for (int i = 0; i < 10; ++i) {
        environment_->addOrganism(createTestOrganism());
    }
    environment_->update(); // Run one generation

    auto stats_before_save = environment_->getStats();
    // With predation disabled, 10 organisms become 11 after reproduction.
    EXPECT_EQ(stats_before_save.population_size, 11);
    EXPECT_EQ(stats_before_save.generation, 1);

    // 2. Save the state
    EXPECT_TRUE(environment_->saveState(save_file));
    ASSERT_TRUE(std::filesystem::exists(save_file));

    // 3. Create a new environment and load the state
    Environment new_environment({});
    EXPECT_TRUE(new_environment.loadState(save_file));

    // 4. Verify the loaded state
    auto stats_after_load = new_environment.getStats();
    EXPECT_EQ(stats_after_load.population_size, stats_before_save.population_size);
    EXPECT_EQ(stats_after_load.generation, stats_before_save.generation);

    // 5. Cleanup
    std::filesystem::remove(save_file);
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

TEST_F(EnvironmentTest, GetTopFittest) {
    // 1. Create organisms with varying fitness scores.
    auto org1 = createTestOrganismWithFitness(0.5);
    auto org2 = createTestOrganismWithFitness(0.9); // Fittest
    auto org3 = createTestOrganismWithFitness(0.2); // Least fit
    auto org4 = createTestOrganismWithFitness(0.7);

    environment_->addOrganism(org1);
    environment_->addOrganism(org2);
    environment_->addOrganism(org3);
    environment_->addOrganism(org4);

    // 2. Get the top 3 fittest organisms.
    auto top_organisms = environment_->getTopFittest(3);

    // 3. Verify the results.
    ASSERT_EQ(top_organisms.size(), 3);
    EXPECT_EQ(top_organisms[0]->getStats().id, org2->getStats().id); // 0.9
    EXPECT_EQ(top_organisms[1]->getStats().id, org4->getStats().id); // 0.7
    EXPECT_EQ(top_organisms[2]->getStats().id, org1->getStats().id); // 0.5
}

} // namespace evosim 