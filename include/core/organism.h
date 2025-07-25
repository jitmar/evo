#pragma once

#include "core/bytecode_vm.h" // Needed for phenotype generation
#include <vector>
#include <memory>
#include <string>
#include <random>
#include <chrono>
#include <atomic>
#include <mutex>
#include "nlohmann/json.hpp"
#include <opencv2/core/mat.hpp>

namespace evosim {

/**
 * @brief Represents a virtual organism with self-replicating capabilities
 * 
 * Each organism contains bytecode that generates an image, and can replicate
 * itself with random mutations to create evolutionary diversity.
 */
class Organism {
public:
    using Bytecode = std::vector<uint8_t>;
    using OrganismPtr = std::shared_ptr<Organism>;
    using ConstOrganismPtr = std::shared_ptr<const Organism>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Image = cv::Mat;

    /**
     * @brief Organism statistics and metadata
     */
    struct Stats {
        uint64_t id;                    ///< Unique organism ID
        uint64_t generation;            ///< Generation number
        uint64_t parent_id;             ///< Parent organism ID
        double fitness_score;           ///< Symmetry-based fitness score
        TimePoint birth_time;           ///< When organism was created
        TimePoint last_replication;     ///< Last replication time
        uint32_t replication_count;     ///< Number of successful replications
        uint32_t mutation_count;        ///< Total mutations accumulated

        Stats(uint64_t id_)
            : id(id_), generation(0), parent_id(0), fitness_score(0.0),
              birth_time(Clock::now()), last_replication(birth_time),
              replication_count(0), mutation_count(0) {}
        Stats() = default;
        Stats(const Stats&) = default;
        Stats(Stats&&) noexcept = default;
        Stats& operator=(const Stats&) = default;
        Stats& operator=(Stats&&) noexcept = default;
        ~Stats() = default;
    };

    /**
     * @brief Constructor for creating a new, random organism.
     * @param vm The BytecodeVM instance to use for generation and execution.
     * @param bytecode_size The size of the bytecode to generate.
     * @param parent_id The ID of the parent organism (0 for initial population).
     */
    Organism(const BytecodeVM& vm, uint32_t bytecode_size, uint64_t parent_id = 0);

    /**
     * @brief Constructor for creating an organism from existing bytecode.
     *
     * Used for replication and loading from state. It executes the provided
     * bytecode to generate the phenotype image.
     *
     * @param bytecode Initial bytecode for the organism
     * @param vm The BytecodeVM instance to use for execution.
     * @param parent_id ID of parent organism (0 for initial organisms)
     */
    Organism(Bytecode bytecode, const BytecodeVM& vm, uint64_t parent_id = 0);

    // Rule of Five
    Organism(const Organism& other);
    Organism(Organism&& other) noexcept;
    Organism& operator=(const Organism& other);
    Organism& operator=(Organism&& other) noexcept;
    ~Organism();
    void swap(Organism& other) noexcept;

    /**
     * @brief Replicate the organism with random mutations
     * @param vm The BytecodeVM instance to use for creating the offspring's phenotype.
     * @param mutation_rate Probability of mutation per byte
     * @param max_mutations Maximum number of mutations per replication
     * @return New organism with mutations
     */
    OrganismPtr replicate(const BytecodeVM& vm, double mutation_rate = 0.01, uint32_t max_mutations = 5) const;

    /**
     * @brief Get organism bytecode
     * @return Const reference to bytecode
     */
    const Bytecode& getBytecode() const;

    /**
     * @brief Update fitness score
     * @param score New fitness score
     */
    void setFitnessScore(double score);

    /**
     * @brief Get fitness score
     * @return Current fitness score
     */
    double getFitnessScore() const;

    /**
     * @brief Get organism statistics
     * @return Stats by value (thread-safe)
     */
    Stats getStats() const;

    /**
     * @brief Get the organism's phenotype (the generated image).
     * @return A const reference to the image.
     */
    const Image& getPhenotype() const;

    /**
     * @brief Get organism age in milliseconds
     * @return Age in milliseconds
     */
    std::chrono::milliseconds getAge() const;

    /**
     * @brief Serialize organism to string
     * @return Serialized organism data
     */
    nlohmann::json serialize() const;

    /**
     * @brief Deserialize organism from string
     * @param vm The BytecodeVM to use for regenerating the phenotype.
     * @param data Serialized organism data
     * @return True if deserialization successful
     */
    bool deserialize(const std::string& data, const BytecodeVM& vm);

private:
    Bytecode bytecode_;                ///< Organism's bytecode
    mutable Stats stats_;              ///< Organism statistics (mutable for replication tracking)
    Image phenotype_;                  ///< The generated image from the bytecode
    mutable std::mutex mutex_;         ///< Thread safety mutex

    static std::atomic<uint64_t> next_id_;  ///< Next available organism ID

    /**
     * @brief Apply random mutations to bytecode
     * @param bytecode Bytecode to mutate
     * @param mutation_rate Mutation probability per byte
     * @param max_mutations Maximum mutations to apply
     * @return Number of mutations applied
     */
    uint32_t applyMutations(Bytecode& bytecode, double mutation_rate, uint32_t max_mutations) const;

    /**
     * @brief Generate random byte
     * @return Random byte value
     */
    uint8_t generateRandomByte() const;
};

} // namespace evosim 