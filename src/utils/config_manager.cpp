#include "utils/config_manager.h"
#include "spdlog/spdlog.h"
#include "yaml-cpp/yaml.h"
#include <fstream>

namespace evosim {

ConfigManager::ConfigManager(const std::string& filepath)
    : filepath_(filepath), config_root_(std::make_unique<YAML::Node>()) {}

ConfigManager::~ConfigManager() = default;

bool ConfigManager::load() {
    try {
        // First, handle the "file not found" case. This is not a fatal error.
        std::ifstream f(filepath_);
        if (!f.good()) {
            spdlog::info("Configuration file not found: '{}'. Using default settings.", filepath_);
            return true;
        }

        // If the file exists, parse it.
        *config_root_ = YAML::LoadFile(filepath_);
        spdlog::info("Successfully loaded configuration from {}", filepath_);
        return true;

    } catch (const YAML::Exception& e) {
        // Handle YAML syntax errors.
        spdlog::error("Failed to parse YAML configuration file '{}': {}", filepath_, e.what());
        return false;
    }
}

// Private helper to safely extract a value from a YAML node.
template <typename T>
void ConfigManager::get_value(const YAML::Node& node, T& out_val) const {
    if (node && !node.IsNull()) {
        try {
            out_val = node.as<T>();
        } catch (const YAML::BadConversion& e) {
            // Log a warning but proceed; the default value will be kept.
            spdlog::warn("YAML type conversion error for a key: {}", e.what());
        }
    }
}

Environment::Config ConfigManager::getEnvironmentConfig() const {
    Environment::Config cfg; // Start with defaults
    if (!config_root_ || !(*config_root_)["environment"]) {
        return cfg; // No environment section, return defaults
    }

    const auto& env_node = (*config_root_)["environment"];
    get_value(env_node["initial_population"], cfg.initial_population);
    get_value(env_node["max_population"], cfg.max_population);
    get_value(env_node["min_population"], cfg.min_population);
    get_value(env_node["mutation_rate"], cfg.mutation_rate);
    get_value(env_node["max_mutations"], cfg.max_mutations);
    get_value(env_node["selection_pressure"], cfg.selection_pressure);
    get_value(env_node["resource_abundance"], cfg.resource_abundance);
    get_value(env_node["generation_time_ms"], cfg.generation_time_ms);
    get_value(env_node["enable_aging"], cfg.enable_aging);
    get_value(env_node["max_age_ms"], cfg.max_age_ms);
    get_value(env_node["enable_competition"], cfg.enable_competition);
    get_value(env_node["competition_intensity"], cfg.competition_intensity);
    get_value(env_node["enable_cooperation"], cfg.enable_cooperation);
    get_value(env_node["cooperation_bonus"], cfg.cooperation_bonus);
    get_value(env_node["enable_predation"], cfg.enable_predation);
    get_value(env_node["enable_random_catastrophes"], cfg.enable_random_catastrophes);

    return cfg;
}

EvolutionEngine::Config ConfigManager::getEvolutionEngineConfig() const {
    EvolutionEngine::Config cfg; // Start with defaults
    if (!config_root_ || !(*config_root_)["evolution_engine"]) {
        return cfg; // No engine section, return defaults
    }

    const auto& engine_node = (*config_root_)["evolution_engine"];
    get_value(engine_node["auto_start"], cfg.auto_start);
    get_value(engine_node["save_interval_generations"], cfg.save_interval_generations);
    get_value(engine_node["save_directory"], cfg.save_directory);
    get_value(engine_node["enable_save_state"], cfg.enable_save_state);
    get_value(engine_node["enable_backup"], cfg.enable_backup);
    get_value(engine_node["backup_interval"], cfg.backup_interval);

    return cfg;
}

BytecodeVM::Config ConfigManager::getBytecodeVMConfig() const {
    BytecodeVM::Config cfg; // Start with defaults
    if (!config_root_ || !(*config_root_)["bytecode_vm"]) {
        return cfg; // No section, return defaults
    }

    const auto& vm_node = (*config_root_)["bytecode_vm"];
    get_value(vm_node["image_width"], cfg.image_width);
    get_value(vm_node["image_height"], cfg.image_height);
    get_value(vm_node["memory_size"], cfg.memory_size);
    get_value(vm_node["stack_size"], cfg.stack_size);
    get_value(vm_node["max_instructions"], cfg.max_instructions);

    return cfg;
}

SymmetryAnalyzer::Config ConfigManager::getSymmetryAnalyzerConfig() const {
    SymmetryAnalyzer::Config cfg; // Start with defaults
    if (!config_root_ || !(*config_root_)["symmetry_analyzer"]) {
        return cfg; // No section, return defaults
    }

    const auto& analyzer_node = (*config_root_)["symmetry_analyzer"];
    get_value(analyzer_node["enable_horizontal"], cfg.enable_horizontal);
    get_value(analyzer_node["enable_vertical"], cfg.enable_vertical);
    get_value(analyzer_node["enable_diagonal"], cfg.enable_diagonal);
    get_value(analyzer_node["enable_rotational"], cfg.enable_rotational);
    get_value(analyzer_node["enable_complexity"], cfg.enable_complexity);
    get_value(analyzer_node["horizontal_weight"], cfg.horizontal_weight);
    get_value(analyzer_node["vertical_weight"], cfg.vertical_weight);
    get_value(analyzer_node["diagonal_weight"], cfg.diagonal_weight);
    get_value(analyzer_node["rotational_weight"], cfg.rotational_weight);
    get_value(analyzer_node["complexity_weight"], cfg.complexity_weight);
    get_value(analyzer_node["histogram_bins"], cfg.histogram_bins);
    get_value(analyzer_node["noise_threshold"], cfg.noise_threshold);
    get_value(analyzer_node["normalize_scores"], cfg.normalize_scores);

    return cfg;
}

} // namespace evosim 