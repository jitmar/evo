#pragma once

#include <vector>
#include <memory>
#include <string>
#include <random>
#include <chrono>
#include <atomic>
#include <mutex>

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
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

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
    };

    /**
     * @brief Constructor
     * @param bytecode Initial bytecode for the organism
     * @param parent_id ID of parent organism (0 for initial organisms)
     */
    explicit Organism(Bytecode bytecode, uint64_t parent_id = 0);

    /**
     * @brief Copy constructor
     */
    Organism(const Organism& other);

    /**
     * @brief Move constructor
     */
    Organism(Organism&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~Organism() = default;

    /**
     * @brief Assignment operator
     */
    Organism& operator=(const Organism& other);

    /**
     * @brief Move assignment operator
     */
    Organism& operator=(Organism&& other) noexcept;

    /**
     * @brief Replicate the organism with random mutations
     * @param mutation_rate Probability of mutation per byte
     * @param max_mutations Maximum number of mutations per replication
     * @return New organism with mutations
     */
    OrganismPtr replicate(double mutation_rate = 0.01, uint32_t max_mutations = 5) const;

    /**
     * @brief Get organism bytecode
     * @return Const reference to bytecode
     */
    const Bytecode& getBytecode() const { return bytecode_; }

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
     * @brief Check if organism is alive (can replicate)
     * @return True if organism can replicate
     */
    bool isAlive() const;

    /**
     * @brief Mark organism as dead
     */
    void die();

    /**
     * @brief Get organism statistics
     * @return Stats by value (thread-safe)
     */
    Stats getStats() const;

    /**
     * @brief Get organism age in milliseconds
     * @return Age in milliseconds
     */
    std::chrono::milliseconds getAge() const;

    /**
     * @brief Serialize organism to string
     * @return Serialized organism data
     */
    std::string serialize() const;

    /**
     * @brief Deserialize organism from string
     * @param data Serialized organism data
     * @return True if deserialization successful
     */
    bool deserialize(const std::string& data);

private:
    Bytecode bytecode_;                ///< Organism's bytecode
    mutable Stats stats_;              ///< Organism statistics (mutable for replication tracking)
    std::atomic<bool> alive_;          ///< Whether organism is alive
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