#include "core/organism.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace evosim {

// Initialize static member
std::atomic<uint64_t> Organism::next_id_{1};

Organism::Organism(Bytecode bytecode, uint64_t parent_id)
    : bytecode_(std::move(bytecode))
    , alive_(true) {
    
    stats_.id = next_id_++;
    stats_.generation = (parent_id == 0) ? 0 : 1;
    stats_.parent_id = parent_id;
    stats_.fitness_score = 0.0;
    stats_.birth_time = Clock::now();
    stats_.last_replication = stats_.birth_time;
    stats_.replication_count = 0;
    stats_.mutation_count = 0;
}

Organism::Organism(const Organism& other)
    : bytecode_(other.bytecode_)
    , alive_(other.alive_.load()) {
    
    stats_.id = next_id_++;
    stats_.generation = other.stats_.generation;
    stats_.parent_id = other.stats_.id;  // This organism is a copy
    stats_.fitness_score = other.stats_.fitness_score;
    stats_.birth_time = Clock::now();
    stats_.last_replication = stats_.birth_time;
    stats_.replication_count = 0;
    stats_.mutation_count = 0;
}

Organism::Organism(Organism&& other) noexcept
    : bytecode_(std::move(other.bytecode_))
    , alive_(other.alive_.load()) {
    
    stats_.id = next_id_++;
    stats_.generation = other.stats_.generation;
    stats_.parent_id = other.stats_.id;
    stats_.fitness_score = other.stats_.fitness_score;
    stats_.birth_time = Clock::now();
    stats_.last_replication = stats_.birth_time;
    stats_.replication_count = 0;
    stats_.mutation_count = 0;
}

Organism& Organism::operator=(const Organism& other) {
    if (this != &other) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::lock_guard<std::mutex> other_lock(other.mutex_);
        
        bytecode_ = other.bytecode_;
        alive_ = other.alive_.load();
        stats_.fitness_score = other.stats_.fitness_score;
        // Don't copy ID, generation, or timestamps
    }
    return *this;
}

Organism& Organism::operator=(Organism&& other) noexcept {
    if (this != &other) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::lock_guard<std::mutex> other_lock(other.mutex_);
        
        bytecode_ = std::move(other.bytecode_);
        alive_ = other.alive_.load();
        stats_.fitness_score = other.stats_.fitness_score;
        // Don't copy ID, generation, or timestamps
    }
    return *this;
}

Organism::OrganismPtr Organism::replicate(double mutation_rate, uint32_t max_mutations) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!alive_) {
        return nullptr;
    }
    
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

std::chrono::milliseconds Organism::getAge() const {
    auto now = Clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - stats_.birth_time);
}

std::string Organism::serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "ORGANISM_V1\n";
    oss << "ID:" << stats_.id << "\n";
    oss << "GENERATION:" << stats_.generation << "\n";
    oss << "PARENT:" << stats_.parent_id << "\n";
    oss << "FITNESS:" << std::fixed << std::setprecision(6) << stats_.fitness_score << "\n";
    oss << "ALIVE:" << (alive_ ? "1" : "0") << "\n";
    oss << "BYTECODE_SIZE:" << bytecode_.size() << "\n";
    oss << "BYTECODE:";
    
    for (uint8_t byte : bytecode_) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    oss << "\n";
    
    return oss.str();
}

bool Organism::deserialize(const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::istringstream iss(data);
    std::string line;
    
    // Check version
    std::getline(iss, line);
    if (line != "ORGANISM_V1") {
        return false;
    }
    
    // Parse fields
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);
        
        if (key == "ID") {
            stats_.id = std::stoull(value);
        } else if (key == "GENERATION") {
            stats_.generation = std::stoull(value);
        } else if (key == "PARENT") {
            stats_.parent_id = std::stoull(value);
        } else if (key == "FITNESS") {
            stats_.fitness_score = std::stod(value);
        } else if (key == "ALIVE") {
            alive_ = (value == "1");
        } else if (key == "BYTECODE_SIZE") {
            size_t size = std::stoul(value);
            bytecode_.reserve(size);
        } else if (key == "BYTECODE") {
            bytecode_.clear();
            for (size_t i = 0; i < value.length(); i += 2) {
                std::string byte_str = value.substr(i, 2);
                uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
                bytecode_.push_back(byte);
            }
        }
    }
    
    return true;
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

} // namespace evosim 