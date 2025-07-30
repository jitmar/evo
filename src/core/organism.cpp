#include "core/organism.h"
#include "core/opcodes.h"
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

uint32_t Organism::applyMutations(Bytecode& bytecode, double mutation_rate, uint32_t max_mutations) const {
    if (bytecode.empty() || mutation_rate <= 0.0 || max_mutations == 0) {
        return 0;
    }
    
    // A list of all opcodes that can be used for mutation.
    // We exclude HALT to prevent mutations from trivially killing the program.
    static const std::vector<Opcode> all_mutable_opcodes = {
        Opcode::NOP, Opcode::PUSH, Opcode::POP, Opcode::ADD, Opcode::SUB,
        Opcode::MUL, Opcode::DIV, Opcode::MOD, Opcode::AND, Opcode::OR,
        Opcode::XOR, Opcode::NOT, Opcode::JMP, Opcode::JZ, Opcode::JNZ,
        Opcode::CALL, Opcode::RET, Opcode::LOAD, Opcode::STORE,
        Opcode::DRAW_PIXEL, Opcode::SET_X, Opcode::SET_Y, Opcode::SET_COLOR_R,
        Opcode::SET_COLOR_G, Opcode::SET_COLOR_B, Opcode::RANDOM, Opcode::DUP,
        Opcode::SWAP, Opcode::ROT, Opcode::DRAW_CIRCLE
    };

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> chance_dist(0.0, 1.0);
    std::uniform_int_distribution<uint8_t> byte_dist(0, 255);
    std::uniform_int_distribution<size_t> opcode_dist(0, all_mutable_opcodes.size() - 1);
    
    uint32_t mutations = 0;
    
    // We iterate by instruction, not by byte, to preserve instruction integrity.
    // We stop before the last byte to protect the final HALT instruction.
    for (size_t i = 0; i < bytecode.size() - 1 && mutations < max_mutations; ) {
        Opcode current_op = static_cast<Opcode>(bytecode[i]);
        int operand_size = getOperandSize(current_op);

        // If opcode is invalid or we're at the end, we can't process a full instruction.
        if (operand_size == -1 || i + static_cast<size_t>(operand_size) >= bytecode.size()) {
            i++;
            continue;
        }

        // Decide if we should mutate this instruction.
        if (chance_dist(rng) < mutation_rate) {
            mutations++;

            constexpr double OPERAND_MUTATION_CHANCE = 0.5;
            bool has_operand = (operand_size > 0);
            bool mutate_operand = has_operand && (chance_dist(rng) < OPERAND_MUTATION_CHANCE);

            if (mutate_operand) {
                const auto op = static_cast<Opcode>(bytecode[i]);
                // --- Smarter Mutation for Jump Instructions ---
                // To prevent infinite loops, we constrain jump targets to always be forward.
                // This ensures the program counter always progresses, forming a DAG.
                if (op == Opcode::JMP || op == Opcode::JZ || op == Opcode::JNZ || op == Opcode::CALL) {
                    // The new target must be after the current instruction.
                    size_t current_instruction_end = i + 1 + static_cast<size_t>(operand_size);
                    // The jump target is a uint8_t, so it's limited to 255.
                    // We also must not jump past the end of the bytecode (or to the final HALT).
                    uint8_t min_target = static_cast<uint8_t>(current_instruction_end);
                    uint8_t max_target = static_cast<uint8_t>(std::min((size_t)255, bytecode.size() - 2));

                    if (min_target <= max_target) {
                        std::uniform_int_distribution<uint8_t> forward_dist(min_target, max_target);
                        bytecode[i + 1] = forward_dist(rng);
                    } else {
                        // Not enough space for a forward jump. Neutralize the instruction to be safe.
                        bytecode[i] = static_cast<uint8_t>(Opcode::NOP);
                    }
                } else {
                    // Standard random mutation for other operands.
                    bytecode[i + 1] = byte_dist(rng);
                }
            } else {
                bytecode[i] = static_cast<uint8_t>(all_mutable_opcodes[opcode_dist(rng)]);
            }
        }
        i += (1 + static_cast<size_t>(operand_size));
    }
    
    return mutations;
}

uint8_t Organism::generateRandomByte() const {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    return dist(rng);
}

}  // namespace evosim