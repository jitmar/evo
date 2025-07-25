#pragma once

#include "core/bytecode_vm.h"
#include "core/symmetry_analyzer.h"
#include "core/environment.h"
#include "core/evolution_engine.h"
#include <string>
#include <memory>

// Forward declare YAML::Node to avoid including the full yaml-cpp header here.
// This is a best practice that improves compile times.
namespace YAML { class Node; }

namespace evosim {

/**
 * @brief Manages loading configuration from YAML files.
 * 
 * This class uses yaml-cpp to parse a configuration file and provides
 * methods to retrieve strongly-typed configuration objects for various
 * components of the simulation.
 */
class ConfigManager {
public:
    explicit ConfigManager(const std::string& filepath);
    ~ConfigManager(); // Required for pimpl with std::unique_ptr

    bool load();

    Environment::Config getEnvironmentConfig() const;
    EvolutionEngine::Config getEvolutionEngineConfig() const;
    BytecodeVM::Config getBytecodeVMConfig() const;
    SymmetryAnalyzer::Config getSymmetryAnalyzerConfig() const;

private:
    // Helper to safely get a value from a YAML node.
    template <typename T>
    void get_value(const YAML::Node& node, T& out_val) const;

    std::string filepath_;
    std::unique_ptr<YAML::Node> config_root_;
};

} // namespace evosim 