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
    using Population = std::vector<OrganismPtr>;
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
        uint32_t min_population;     ///< Minimum population size
        double mutation_rate;        ///< Base mutation rate
        uint32_t max_mutations;      ///< Maximum mutations per replication
        double selection_pressure;   ///< Selection pressure (0-1)
        double resource_abundance;   ///< Resource abundance multiplier
        uint32_t generation_time_ms; ///< Generation time in milliseconds
        bool enable_aging;           ///< Enable organism aging
        uint32_t max_age_ms;         ///< Maximum organism age in milliseconds
        bool enable_competition;     ///< Enable organism competition
        double competition_intensity; ///< Competition intensity (0-1)
        bool enable_cooperation;     ///< Enable organism cooperation
        double cooperation_bonus;    ///< Cooperation fitness bonus
        
        Config() : max_population(1000), initial_population(100), min_population(10),
                   mutation_rate(0.01), max_mutations(5), selection_pressure(0.7),
                   resource_abundance(1.0), generation_time_ms(1000), enable_aging(true),
                   max_age_ms(30000), enable_competition(true), competition_intensity(0.5),
                   enable_cooperation(false), cooperation_bonus(0.1) {}
    };

    /**
     * @brief Constructor
     * @param config Environment configuration
     */
    explicit Environment(const Config& config = Config());

    /**
     * @brief Destructor
     */
    ~Environment() = default;

    /**
     * @brief Initialize environment with random organisms
     * @param bytecode_size Size of initial organism bytecode
     */
    void initialize(uint32_t bytecode_size = 64);

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
    std::shared_ptr<Organism> getOrganism(uint64_t organism_id) const;

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
    const Config& getConfig() const { return config_; }

    /**
     * @brief Set environment configuration
     * @param config New configuration
     */
    void setConfig(const Config& config) { config_ = config; }

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
    std::vector<std::shared_ptr<Organism>> selectForReproduction(uint32_t count) const;

    /**
     * @brief Perform natural selection
     * @return Number of organisms that died
     */
    uint32_t performSelection();

    /**
     * @brief Generate new organisms through reproduction
     * @return Number of new organisms created
     */
    uint32_t performReproduction();

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
    std::shared_ptr<Organism> getBestOrganism() const;

    /**
     * @brief Get organism statistics
     * @return Vector of organism statistics
     */
    std::vector<Organism::Stats> getOrganismStats() const;

private:
    Config config_;                     ///< Environment configuration
    Population population_;             ///< Current organism population
    EnvironmentStats stats_;            ///< Environment statistics
    BytecodeVM vm_;                    ///< Bytecode virtual machine
    SymmetryAnalyzer analyzer_;         ///< Symmetry analyzer
    mutable std::mt19937 rng_;          ///< Random number generator
    mutable std::mutex mutex_;          ///< Thread safety mutex

    /**
     * @brief Generate random bytecode
     * @param size Bytecode size
     * @return Random bytecode
     */
    Organism::Bytecode generateRandomBytecode(uint32_t size) const;

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

    /**
     * @brief Sort population by fitness
     */
    void sortPopulationByFitness();
};

} // namespace evosim 