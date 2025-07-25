#include "test_common.h"
#include "utils/config_manager.h"
#include <fstream>
#include <filesystem>

namespace evosim {

class ConfigManagerTest : public ::testing::Test {
protected:
    void TearDown() override {
        // Clean up any test files that might have been created.
        if (std::filesystem::exists("test_config.yaml")) {
            std::filesystem::remove("test_config.yaml");
        }
        if (std::filesystem::exists("malformed_config.yaml")) {
            std::filesystem::remove("malformed_config.yaml");
        }
    }

    void createTestConfigFile(const std::string& filename, const std::string& content) {
        std::ofstream file(filename);
        file << content;
        file.close();
    }
};

TEST_F(ConfigManagerTest, LoadValidConfigFile) {
    const std::string content = R"(
environment:
  initial_population: 150
  mutation_rate: 0.05
  enable_predation: false

evolution_engine:
  save_interval_generations: 50
  save_directory: "test_saves"

bytecode_vm:
  image_width: 128
  max_instructions: 5000
)";
    createTestConfigFile("test_config.yaml", content);

    ConfigManager manager("test_config.yaml");
    ASSERT_TRUE(manager.load());

    auto env_config = manager.getEnvironmentConfig();
    EXPECT_EQ(env_config.initial_population, 150);
    EXPECT_DOUBLE_EQ(env_config.mutation_rate, 0.05);
    EXPECT_FALSE(env_config.enable_predation);
    // Check a default value was not overwritten
    EXPECT_TRUE(env_config.enable_aging);

    auto engine_config = manager.getEvolutionEngineConfig();
    EXPECT_EQ(engine_config.save_interval_generations, 50);
    EXPECT_EQ(engine_config.save_directory, "test_saves");
    // Check a default value
    EXPECT_TRUE(engine_config.enable_save_state);

    auto vm_config = manager.getBytecodeVMConfig();
    EXPECT_EQ(vm_config.image_width, 128);
    EXPECT_EQ(vm_config.max_instructions, 5000);
    // Check a default value was not overwritten
    EXPECT_EQ(vm_config.stack_size, 256);
}

TEST_F(ConfigManagerTest, LoadMissingConfigFile) {
    ConfigManager manager("non_existent_file.yaml");
    ASSERT_TRUE(manager.load()); // Should succeed and use defaults

    // Verify that default values are used
    auto env_config = manager.getEnvironmentConfig();
    Environment::Config default_env_config;
    EXPECT_EQ(env_config.initial_population, default_env_config.initial_population);
    EXPECT_DOUBLE_EQ(env_config.mutation_rate, default_env_config.mutation_rate);

    auto engine_config = manager.getEvolutionEngineConfig();
    EvolutionEngine::Config default_engine_config;
    EXPECT_EQ(engine_config.save_interval_generations, default_engine_config.save_interval_generations);
}

TEST_F(ConfigManagerTest, LoadMalformedConfigFile) {
    const std::string content = R"(
environment:
  initial_population: 150
    bad_indent: true
)";
    createTestConfigFile("malformed_config.yaml", content);

    ConfigManager manager("malformed_config.yaml");
    ASSERT_FALSE(manager.load()); // Should fail to parse
}

} // namespace evosim