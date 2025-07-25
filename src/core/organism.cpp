#include "core/organism.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>
#include "spdlog/spdlog.h"
#include "nlohmann/json.hpp"

// TODO: For high-performance scenarios, consider using dedicated mutexes for independent properties
// instead of a single mutex_ for the entire Organism. This would allow greater concurrency at the
// cost of increased complexity and potential deadlocks. For now, a single mutex is used for safety and simplicity.

namespace evosim {

// Initialize static member
std::atomic<uint64_t> Organism::next_id_{1};

Organism::Organism(Bytecode bytecode, uint64_t parent_id)
    : bytecode_(std::move(bytecode)),
      stats_(next_id_++)
{
    // Only set fields not already set by Stats(uint64_t id_)
    stats_.generation = (parent_id == 0) ? 0 : 1;
    stats_.parent_id = parent_id;
}

Organism::Organism(const Organism& other)
    : bytecode_(other.bytecode_),
      stats_(other.stats_)
{
    // Copy constructor: assign new unique id, set parent_id to original's id
    stats_.id = next_id_++;
    stats_.parent_id = other.stats_.id;
    stats_.birth_time = Clock::now();
    stats_.last_replication = stats_.birth_time;
    stats_.replication_count = 0;
    stats_.mutation_count = other.stats_.mutation_count;
    // generation, fitness_score, etc. are copied as-is
}

Organism::Organism(Organism&& other) noexcept
    : bytecode_(std::move(other.bytecode_)),
      stats_(std::move(other.stats_))
{
    // Move constructor
}

Organism& Organism::operator=(const Organism& other) {
    if (this != &other) {
        Organism temp(other); // invokes copy constructor (assigns new id, etc.)
        swap(temp);
    }
    return *this;
}

Organism& Organism::operator=(Organism&& other) noexcept {
    if (this != &other) {
        Organism temp(std::move(other)); // invokes move constructor (preserves id)
        swap(temp);
    }
    return *this;
}

Organism::~Organism() = default;

void Organism::swap(Organism& other) noexcept
{
    using std::swap;
    swap(bytecode_, other.bytecode_);
    swap(stats_, other.stats_);
    // Add swap for any other members if needed
}

Organism::OrganismPtr Organism::replicate(double mutation_rate, uint32_t max_mutations) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create new bytecode with mutations
    Bytecode new_bytecode = bytecode_;
    uint32_t mutations_applied = applyMutations(new_bytecode, mutation_rate, max_mutations);
    
    // Create new organism
    auto child = std::make_shared<Organism>(new_bytecode, stats_.id);
    child->stats_.mutation_count = mutations_applied;
    child->stats_.generation = stats_.generation + 1;
    
    // Update parent stats
    stats_.replication_count++;
    stats_.last_replication = Clock::now();
    
    return child;
}

void Organism::setFitnessScore(double score) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.fitness_score = score;
}

double Organism::getFitnessScore() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_.fitness_score;
}

std::chrono::milliseconds Organism::getAge() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = Clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - stats_.birth_time);
}

Organism::Stats Organism::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

nlohmann::json Organism::serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);

    nlohmann::json organism_json;
    organism_json["id"] = stats_.id;
    organism_json["generation"] = stats_.generation;
    organism_json["parent_id"] = stats_.parent_id;
    organism_json["fitness_score"] = stats_.fitness_score;
    organism_json["bytecode"] = bytecode_;

    return organism_json;
}

bool Organism::deserialize(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        nlohmann::json organism_json = nlohmann::json::parse(data);
        stats_.id = organism_json["id"];
        stats_.generation = organism_json["generation"];
        stats_.parent_id = organism_json["parent_id"];
        stats_.fitness_score = organism_json["fitness_score"];
        bytecode_ = organism_json["bytecode"].get<std::vector<uint8_t>>();
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to deserialize organism: {}", e.what());
        return false;
    }
}

uint32_t Organism::applyMutations(Bytecode& bytecode, double mutation_rate, uint32_t max_mutations) const {
    if (bytecode.empty() || mutation_rate <= 0.0 || max_mutations == 0) {
        return 0;
    }
    
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
    
    uint32_t mutations = 0;
    
    for (size_t i = 0; i < bytecode.size() && mutations < max_mutations; ++i) {
        if (dist(rng) < mutation_rate) {
            bytecode[i] = byte_dist(rng);
            mutations++;
        }
    }
    
    return mutations;
}

uint8_t Organism::generateRandomByte() const {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    return dist(rng);
}

}  // namespace evosim