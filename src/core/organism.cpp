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

Organism::Organism(const BytecodeVM& vm, uint32_t bytecode_size, uint64_t parent_id)
    : bytecode_(generatePrimordialBytecode(bytecode_size)),
      stats_(next_id_++),
      phenotype_(vm.execute(bytecode_))
{
    stats_.generation = (parent_id == 0) ? 0 : 1;
    stats_.parent_id = parent_id;
}

Organism::Organism(Bytecode bytecode, const BytecodeVM& vm, uint64_t parent_id)
    : bytecode_(std::move(bytecode)),
      stats_(next_id_++)
{
    // Only set fields not already set by Stats(uint64_t id_)
    stats_.generation = (parent_id == 0) ? 0 : 1;
    stats_.parent_id = parent_id;
    // Generate the phenotype from the provided bytecode
    phenotype_ = vm.execute(bytecode_);
}

Organism::Organism(const Organism& other)
    : bytecode_(other.bytecode_),
      stats_(other.stats_),
      phenotype_(other.phenotype_.clone())
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
      stats_(std::move(other.stats_)),
      phenotype_(std::move(other.phenotype_))
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
    swap(phenotype_, other.phenotype_);
}

Organism::OrganismPtr Organism::replicate(const BytecodeVM& vm, double mutation_rate, uint32_t max_mutations) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create new bytecode with mutations
    Bytecode new_bytecode = bytecode_;
    uint32_t mutations_applied = applyMutations(new_bytecode, mutation_rate, max_mutations);
    
    // Create new organism, passing the vm to generate its phenotype
    auto child = std::make_shared<Organism>(new_bytecode, vm, stats_.id);
    child->stats_.mutation_count = mutations_applied;
    child->stats_.generation = stats_.generation + 1;
    
    // Update parent stats
    stats_.replication_count++;
    stats_.last_replication = Clock::now();
    
    return child;
}

const Organism::Bytecode& Organism::getBytecode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bytecode_;
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

const Organism::Image& Organism::getPhenotype() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return phenotype_;
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

bool Organism::deserialize(const std::string& data, const BytecodeVM& vm) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        nlohmann::json organism_json = nlohmann::json::parse(data);
        stats_.id = organism_json["id"];
        stats_.generation = organism_json["generation"];
        stats_.parent_id = organism_json["parent_id"];
        stats_.fitness_score = organism_json["fitness_score"];
        bytecode_ = organism_json["bytecode"].get<std::vector<uint8_t>>();

        // Regenerate the phenotype after loading the bytecode
        phenotype_ = vm.execute(bytecode_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to deserialize organism: {}", e.what());
        return false;
    }
}

Organism::Bytecode Organism::generatePrimordialBytecode(uint32_t size) {
    // This function creates a "primordial soup" that is more likely to produce
    // interesting (i.e., non-black) phenotypes from the start.
    
    Bytecode code(size);
    static thread_local std::mt19937 rng(std::random_device{}());

    // 1. Fill with completely random bytes first to provide evolutionary raw material.
    std::uniform_int_distribution<uint8_t> dist_byte(0, 255);
    for (uint8_t& byte : code) {
        byte = dist_byte(rng);
    }

    // 2. "Sprinkle" in known useful instruction sequences to guarantee color and drawing.
    std::uniform_int_distribution<uint8_t> dist_color(50, 255); // Avoid dark colors
    std::uniform_int_distribution<uint8_t> dist_pos(32, 224);   // Avoid image edges
    std::uniform_int_distribution<uint8_t> dist_size(16, 64);
    std::uniform_int_distribution<size_t> dist_loc(0, size > 1 ? size - 1 : 0);

    // Inject a "set color" sequence at a random location.
    // Sequence: PUSH R, SET_COLOR_R, PUSH G, SET_COLOR_G, PUSH B, SET_COLOR_B (9 bytes)
    if (size >= 9) {
        size_t loc = dist_loc(rng) % (size - 8);
        code[loc++] = static_cast<uint8_t>(BytecodeVM::Opcode::PUSH);
        code[loc++] = dist_color(rng); // R
        code[loc++] = static_cast<uint8_t>(BytecodeVM::Opcode::SET_COLOR_R);
        code[loc++] = static_cast<uint8_t>(BytecodeVM::Opcode::PUSH);
        code[loc++] = dist_color(rng); // G
        code[loc++] = static_cast<uint8_t>(BytecodeVM::Opcode::SET_COLOR_G);
        code[loc++] = static_cast<uint8_t>(BytecodeVM::Opcode::PUSH);
        code[loc++] = dist_color(rng); // B
        code[loc]   = static_cast<uint8_t>(BytecodeVM::Opcode::SET_COLOR_B);
    }

    // Inject a "draw circle" sequence at another random location.
    // Sequence: SET_X, X, SET_Y, Y, PUSH, Radius, DRAW_CIRCLE (7 bytes)
    if (size >= 7) {
        size_t loc = dist_loc(rng) % (size - 6);
        code[loc++] = static_cast<uint8_t>(BytecodeVM::Opcode::SET_X);
        code[loc++] = dist_pos(rng);  // X
        code[loc++] = static_cast<uint8_t>(BytecodeVM::Opcode::SET_Y);
        code[loc++] = dist_pos(rng);  // Y
        code[loc++] = static_cast<uint8_t>(BytecodeVM::Opcode::PUSH);
        code[loc++] = dist_size(rng); // Radius
        code[loc]   = static_cast<uint8_t>(BytecodeVM::Opcode::DRAW_CIRCLE);
    }

    return code;
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