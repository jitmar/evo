#pragma once

#include "organism.h"
#include "bytecode_vm.h"
#include "symmetry_analyzer.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>
#include "nlohmann/json.hpp"

namespace evosim {

/**
 * @brief Virtual environment for organism evolution
 * 
 * The environment manages organism populations, provides resources,
 * and controls evolutionary pressures and selection mechanisms.
 */
class Environment {
public:
    using OrganismPtr = Organism::OrganismPtr;
    using ConstOrganismPtr = std::shared_ptr<const Organism>;
    using Population = std::unordered_map<uint64_t, std::shared_ptr<Organism>>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    /**
     * @brief Environment statistics
     */
    struct EnvironmentStats {
        uint64_t generation;            ///< Current generation
        uint32_t population_size;       ///< Current population size
        uint32_t max_population;        ///< Maximum population size
        uint32_t births_this_gen;       ///< Births in current generation
        uint32_t deaths_this_gen;       ///< Deaths in current generation
        double avg_fitness;             ///< Average fitness score
        double max_fitness;             ///< Maximum fitness score
        double min_fitness;             ///< Minimum fitness score
        double fitness_variance;        ///< Fitness variance
        // TimePoint last_update is not serialized as it's transient state.
        TimePoint last_update;          ///< Last environment update
        uint64_t total_organisms_created; ///< Total organisms ever created
        uint64_t total_organisms_died;  ///< Total organisms that died
    };

    /**
     * @brief Environment configuration
     */
    struct Config {
        uint32_t max_population;     ///< Maximum population size
        uint32_t initial_population; ///< Initial population size
        uint32_t initial_bytecode_size; ///< The initial size of bytecode for new organisms.
        uint32_t min_population;     ///< Minimum population size
        uint32_t elite_count;        ///< Number of fittest organisms to preserve each generation.
        double mutation_rate;        ///< Base mutation rate
        uint32_t max_mutations;      ///< Maximum mutations per replication
        double resource_abundance;   ///< Resource abundance multiplier
        uint32_t generation_time_ms; ///< Generation time in milliseconds
        bool enable_aging;           ///< Enable organism aging
        uint32_t max_age_ms;         ///< Maximum organism age in milliseconds
        bool enable_competition;     ///< Enable organism competition
        double competition_intensity; ///< Competition intensity (0-1)
        bool enable_cooperation;     ///< Enable organism cooperation
        double cooperation_bonus;    ///< Cooperation fitness bonus
        bool enable_predation;       ///< Enable organism predation
        bool enable_random_catastrophes; ///< Enable random catastrophe events

        // --- Fitness Function Weights ---
        // These weights determine the contribution of different high-level metrics
        // to the final fitness score. They should ideally sum to 1.0.
        double fitness_weight_symmetry;    ///< Weight for the combined score from SymmetryAnalyzer (symmetry, complexity, etc.).
        double fitness_weight_variation;   ///< Weight for the color variation score (rewards non-monochrome images).

        Config()
            : max_population(1000), initial_population(100),
              initial_bytecode_size(64), min_population(10), elite_count(2),
              mutation_rate(0.01), max_mutations(5),
              resource_abundance(1.0), generation_time_ms(1000),
              enable_aging(true), max_age_ms(30000), enable_competition(true),
              competition_intensity(0.5), enable_cooperation(false), cooperation_bonus(0.1),
              enable_predation(true), enable_random_catastrophes(true),
              fitness_weight_symmetry(0.6), fitness_weight_variation(0.4) {
        }
    };

    /**
     * @brief Constructor
     * @param config Environment configuration
     */
    explicit Environment(
        const Config& config = Config(),
        const BytecodeVM::Config& vm_config = {},
        const SymmetryAnalyzer::Config& analyzer_config = {}
    );

    /**
     * @brief Destructor
     */
    ~Environment() = default;

    /**
     * @brief Initialize environment with random organisms
     */
    void initialize();

    /**
     * @brief Update environment for one generation
     * @return True if update successful
     */
    bool update();

    /**
     * @brief Add organism to environment
     * @param organism Organism to add
     * @return True if successful
     */
    bool addOrganism(OrganismPtr organism);

    /**
     * @brief Remove organism from environment
     * @param organism_id ID of organism to remove
     * @return True if successful
     */
    bool removeOrganism(uint64_t organism_id);

    /**
     * @brief Get organism by ID
     * @param organism_id Organism ID
     * @return Organism pointer or nullptr if not found
     */
    ConstOrganismPtr getOrganism(uint64_t organism_id) const;

    /**
     * @brief Get current population
     * @return Const reference to population
     */
    const Population& getPopulation() const { return population_; }

    /**
     * @brief Get environment statistics
     * @return Current environment statistics
     */
    EnvironmentStats getStats() const;

    /**
     * @brief Get environment configuration
     * @return Current configuration
     */
    Config getConfig() const;

    /**
     * @brief Set environment configuration
     * @param config New configuration
     */
    void setConfig(const Config& config) { config_ = config; }

    /**
     * @brief Set the configuration of the internal BytecodeVM.
     * @param config The new configuration for the BytecodeVM.
     */
    void setVMConfig(const BytecodeVM::Config& config);

    /**
     * @brief Set the configuration of the internal SymmetryAnalyzer.
     * @param config The new configuration for the SymmetryAnalyzer.
     */
    void setAnalyzerConfig(const SymmetryAnalyzer::Config& config);

    /**
     * @brief Get the configuration of the internal BytecodeVM.
     * @return The BytecodeVM's configuration object.
     */
    BytecodeVM::Config getVMConfig() const;

    /**
     * @brief Get the configuration of the internal SymmetryAnalyzer.
     * @return The SymmetryAnalyzer's configuration object.
     */
    SymmetryAnalyzer::Config getAnalyzerConfig() const;

    /**
     * @brief Get the full configuration of the environment and its components.
     * @return A JSON object containing all configuration data.
     */
    nlohmann::json getFullConfig() const;

    /**
     * @brief Evaluate organism fitness
     * @param organism Organism to evaluate
     * @return Fitness score
     */
    double evaluateFitness(const OrganismPtr& organism);

    /**
     * @brief Select organisms for reproduction
     * @param count Number of organisms to select
     * @return Selected organisms
     */
    std::vector<OrganismPtr> selectForReproduction(uint32_t count) const;

    /**
     * @brief Perform natural selection
     * @return Number of organisms that died
     */
    uint32_t performSelection();

    /**
     * @brief Generate new organisms through reproduction
     * @param reproduction_pool A pre-sorted list of organisms eligible to be parents.
     * @return Number of new organisms created
     */
    uint32_t performReproduction(const std::vector<OrganismPtr>& reproduction_pool);

    /**
     * @brief Apply environmental pressures
     */
    void applyEnvironmentalPressures();

    /**
     * @brief Save environment state
     * @param filename Output filename
     * @return True if successful
     */
    bool saveState(const std::string& filename) const;

    /**
     * @brief Load environment state
     * @param filename Input filename
     * @return True if successful
     */
    bool loadState(const std::string& filename);

    /**
     * @brief Clear environment
     */
    void clear();

    /**
     * @brief Get best organism
     * @return Organism with highest fitness
     */
    ConstOrganismPtr getBestOrganism() const;

    /**
     * @brief Get organism statistics
     * @return Vector of organism statistics
     */
    std::vector<Organism::Stats> getOrganismStats() const;

    /**
     * @brief Get the top N fittest organisms.
     * @param count The number of organisms to retrieve.
     * @return A vector of the top N fittest organisms, sorted by fitness.
     */
    std::vector<ConstOrganismPtr> getTopFittest(uint32_t count) const;

private:
    Config config_;                     ///< Environment configuration
    Population population_;             ///< Current organism population
    EnvironmentStats stats_;            ///< Environment statistics
    BytecodeVM vm_;                    ///< Bytecode virtual machine
    SymmetryAnalyzer analyzer_;         ///< Symmetry analyzer
    mutable std::mt19937 rng_;          ///< Random number generator
    mutable std::mutex mutex_;          ///< Thread safety mutex

    void apply_resource_scarcity_();
    void apply_random_catastrophe_();
    void apply_predation_();
    void remove_random_organisms_unlocked_(uint32_t count);
    void apply_environmental_pressures_unlocked_();

    /**
     * @brief Calculate fitness-based selection probability
     * @param fitness Organism fitness score
     * @return Selection probability
     */
    double calculateSelectionProbability(double fitness) const;

    /**
     * @brief Update environment statistics
     */
    void updateStats();

    /**
     * @brief Apply aging effects
     */
    void applyAging();

    /**
     * @brief Apply competition effects
     */
    void applyCompetition();

    /**
     * @brief Apply cooperation effects
     */
    void applyCooperation();

    /**
     * @brief Calculate population fitness statistics
     */
    void calculateFitnessStats();

    std::vector<OrganismPtr> select_for_reproduction_unlocked_(uint32_t count) const;
};

inline void to_json(nlohmann::json& j, const Environment::Config& c) {
    j = nlohmann::json{
        {"max_population", c.max_population},
        {"initial_population", c.initial_population},
        {"initial_bytecode_size", c.initial_bytecode_size},
        {"min_population", c.min_population},
        {"elite_count", c.elite_count},
        {"mutation_rate", c.mutation_rate},
        {"max_mutations", c.max_mutations},
        {"resource_abundance", c.resource_abundance},
        {"generation_time_ms", c.generation_time_ms},
        {"enable_aging", c.enable_aging},
        {"max_age_ms", c.max_age_ms},
        {"enable_competition", c.enable_competition},
        {"competition_intensity", c.competition_intensity},
        {"enable_cooperation", c.enable_cooperation},
        {"cooperation_bonus", c.cooperation_bonus},
        {"enable_predation", c.enable_predation},
        {"enable_random_catastrophes", c.enable_random_catastrophes},
        {"fitness_weight_symmetry", c.fitness_weight_symmetry},
        {"fitness_weight_variation", c.fitness_weight_variation}
    };
}

inline void from_json(const nlohmann::json& j, Environment::Config& c) {
    j.at("max_population").get_to(c.max_population);
    j.at("initial_population").get_to(c.initial_population);
    j.at("initial_bytecode_size").get_to(c.initial_bytecode_size);
    j.at("min_population").get_to(c.min_population);
    j.at("elite_count").get_to(c.elite_count);
    j.at("mutation_rate").get_to(c.mutation_rate);
    j.at("max_mutations").get_to(c.max_mutations);
    j.at("resource_abundance").get_to(c.resource_abundance);
    j.at("generation_time_ms").get_to(c.generation_time_ms);
    j.at("enable_aging").get_to(c.enable_aging);
    j.at("max_age_ms").get_to(c.max_age_ms);
    j.at("enable_competition").get_to(c.enable_competition);
    j.at("competition_intensity").get_to(c.competition_intensity);
    j.at("enable_cooperation").get_to(c.enable_cooperation);
    j.at("cooperation_bonus").get_to(c.cooperation_bonus);
    j.at("enable_predation").get_to(c.enable_predation);
    j.at("enable_random_catastrophes").get_to(c.enable_random_catastrophes);
    j.at("fitness_weight_symmetry").get_to(c.fitness_weight_symmetry);
    j.at("fitness_weight_variation").get_to(c.fitness_weight_variation);
}

inline void to_json(nlohmann::json& j, const Environment::EnvironmentStats& s) {
    j = nlohmann::json{
        {"generation", s.generation},
        {"population_size", s.population_size},
        {"max_population", s.max_population},
        {"births_this_gen", s.births_this_gen},
        {"deaths_this_gen", s.deaths_this_gen},
        {"avg_fitness", s.avg_fitness},
        {"max_fitness", s.max_fitness},
        {"min_fitness", s.min_fitness},
        {"fitness_variance", s.fitness_variance},
        {"total_organisms_created", s.total_organisms_created},
        {"total_organisms_died", s.total_organisms_died}};
}

inline void from_json(const nlohmann::json& j, Environment::EnvironmentStats& s) {
    j.at("generation").get_to(s.generation);
    j.at("population_size").get_to(s.population_size);
    j.at("max_population").get_to(s.max_population);
    j.at("births_this_gen").get_to(s.births_this_gen);
    j.at("deaths_this_gen").get_to(s.deaths_this_gen);
    j.at("avg_fitness").get_to(s.avg_fitness);
    j.at("max_fitness").get_to(s.max_fitness);
    j.at("min_fitness").get_to(s.min_fitness);
    j.at("fitness_variance").get_to(s.fitness_variance);
    j.at("total_organisms_created").get_to(s.total_organisms_created);
    j.at("total_organisms_died").get_to(s.total_organisms_died);
}

} // namespace evosim 