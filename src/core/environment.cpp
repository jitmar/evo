#include "core/environment.h"
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include <numeric>
#include <unordered_set> // Added for apply_predation_
#include <iostream>
#include <cmath>
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

namespace evosim {

Environment::Environment(
    const Config& config,
    const BytecodeVM::Config& vm_config,
    const SymmetryAnalyzer::Config& analyzer_config
)
    : config_(config)
    , vm_(vm_config)
    , analyzer_(analyzer_config)
    , rng_(std::random_device{}()) {
    // Pass the configured initial bytecode size from the environment's config.
    initialize(config_.initial_bytecode_size);
}

void Environment::initialize(uint32_t bytecode_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    population_.clear();
    stats_ = EnvironmentStats{};
    
    // Generate initial population
    for (uint32_t i = 0; i < config_.initial_population; ++i) {
        auto bytecode = generateRandomBytecode(bytecode_size);
        auto organism = std::make_shared<Organism>(bytecode, 0);
        population_[organism->getStats().id] = organism;
        stats_.total_organisms_created++;
    }
    
    updateStats();
}

bool Environment::update() {
    try {
        // Step 1: Get a snapshot of the current population without holding the lock for too long.
        std::vector<OrganismPtr> current_population;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (population_.empty()) {
                return true; // Nothing to do.
            }
            current_population.reserve(population_.size());
            for (const auto& pair : population_) {
                if (pair.second) {
                    current_population.push_back(pair.second);
                }
            }
        }

        // Step 2: Perform the expensive fitness evaluation WITHOUT holding the main environment lock.
        // The Organism's setFitnessScore method is individually thread-safe.
        for (const auto& organism : current_population) {
            double fitness = evaluateFitness(organism);
            organism->setFitnessScore(fitness);
        }

        // Step 3: Re-acquire the lock to perform state-modifying operations.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            apply_environmental_pressures_unlocked_();
            uint32_t deaths = performSelection();
            stats_.deaths_this_gen = deaths;
            uint32_t births = performReproduction();
            stats_.births_this_gen = births;
            stats_.generation++;
            stats_.last_update = Clock::now();
            updateStats();
        }
        return true;
    } catch (const std::exception& e) {
        // It's safer to lock here as well, in case the exception was thrown from a non-locked part.
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::error("Exception during environment update: {}", e.what());
        return false;
    }
}

bool Environment::addOrganism(OrganismPtr organism) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!organism || population_.size() >= config_.max_population) {
        return false;
    }
    
    population_[organism->getStats().id] = organism;
    stats_.total_organisms_created++;
    updateStats();
    
    return true;
}

bool Environment::removeOrganism(uint64_t organism_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = population_.find(organism_id);
    
    if (it != population_.end()) {
        population_.erase(it);
        stats_.total_organisms_died++;
        updateStats();
        return true;
    }
    
    return false;
}

std::shared_ptr<Organism> Environment::getOrganism(uint64_t organism_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = population_.find(organism_id);
    
    return (it != population_.end()) ? it->second : nullptr;
}

Environment::EnvironmentStats Environment::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

Environment::Config Environment::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

BytecodeVM::Config Environment::getVMConfig() const {
    return vm_.getConfig();
}

SymmetryAnalyzer::Config Environment::getAnalyzerConfig() const {
    return analyzer_.getConfig();
}

double Environment::evaluateFitness(const OrganismPtr& organism) {
    if (!organism) return 0.0;
    
    // Generate image from organism's bytecode
    auto image = vm_.execute(organism->getBytecode());
    
    // --- Component 1: Analyzer Score ---
    // The SymmetryAnalyzer computes a detailed, weighted fitness score based on its
    // own configuration (horizontal, vertical, rotational symmetry, complexity, etc.).
    // We use this as the primary component of our final fitness.
    auto analysis_result = analyzer_.analyze(image);
    double analyzer_score = analysis_result.fitness_score;

    // --- Component 2: Color Variation Score ---
    // This component rewards images that are not blank or monochrome, preventing
    // evolution from getting stuck on trivial solutions. We calculate the standard
    // deviation of the pixel values across all channels.
    cv::Scalar mean, stddev;
    cv::meanStdDev(image, mean, stddev);
    // Average the standard deviation across B, G, R channels and normalize.
    // For an 8-bit image, max stddev is ~127.5. We divide by a slightly larger
    // value and cap at 1.0 to keep it in a standard [0, 1] range.
    double variation_score = std::min(1.0, (stddev[0] + stddev[1] + stddev[2]) / 3.0 / 128.0);

    // --- Final Weighted Combination ---
    // The final fitness is a weighted sum of the high-level component scores.
    // This allows us to balance the drive for symmetry/complexity against the
    // need for basic color variation.
    double final_fitness = (config_.fitness_weight_symmetry * analyzer_score) +
                           (config_.fitness_weight_variation * variation_score);

    return final_fitness;
}

std::vector<std::shared_ptr<Organism>> Environment::selectForReproduction(uint32_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return select_for_reproduction_unlocked_(count);
}

std::vector<std::shared_ptr<Organism>> Environment::select_for_reproduction_unlocked_(uint32_t count) const {
    if (population_.empty()) {
        return {};
    }
    // Sort by fitness (descending)
    std::vector<std::shared_ptr<Organism>> sorted_population;
    sorted_population.reserve(population_.size());
    for (const auto& pair : population_) {
        if (pair.second) {
            sorted_population.push_back(pair.second);
        }
    }
    std::sort(sorted_population.begin(), sorted_population.end(),
        [](const std::shared_ptr<Organism>& a, const std::shared_ptr<Organism>& b) {
            if (!a) return false;
            if (!b) return true;
            return a->getFitnessScore() > b->getFitnessScore();
        });
    // Select top organisms
    count = std::min(count, static_cast<uint32_t>(sorted_population.size()));
    auto result = std::vector<std::shared_ptr<Organism>>(sorted_population.begin(), sorted_population.begin() + count);
    return result;
}

uint32_t Environment::performSelection() {
    auto initial_size = static_cast<uint32_t>(population_.size());
    // Remove dead organisms (now: just erase nullptrs, but should be empty)
    for (auto it = population_.begin(); it != population_.end(); ) {
        if (!it->second) {
            it = population_.erase(it);
        } else {
            ++it;
        }
    }
    // Apply aging if enabled
    if (config_.enable_aging) {
        applyAging();
    }
    // Apply competition if enabled
    if (config_.enable_competition) {
        applyCompetition();
    }
    // Apply cooperation if enabled
    if (config_.enable_cooperation) {
        applyCooperation();
    }
    auto final_size = static_cast<uint32_t>(population_.size());
    return initial_size - final_size;
}

uint32_t Environment::performReproduction() {
    (void)population_.size(); // Suppress unused variable warning
    uint32_t target_size = std::min(config_.max_population, 
                                   std::max(config_.min_population,
                                           static_cast<uint32_t>(std::ceil(static_cast<double>(population_.size()) * 1.1))));
    std::vector<OrganismPtr> new_organisms;
    const size_t max_iterations = 10 * target_size; // Safety limit
    size_t iterations = 0;

    while (population_.size() + new_organisms.size() < target_size && iterations < max_iterations) {
        ++iterations;
        // Use unlocked helper to avoid deadlock
        auto parents = select_for_reproduction_unlocked_(1);
        auto parent = parents[0];
        if (!parent) {
            continue;
        }
        auto offspring = parent->replicate(config_.mutation_rate, config_.max_mutations);
        if (!offspring) {
            continue;
        }
        new_organisms.push_back(offspring);
        stats_.total_organisms_created++;
    }
    if (iterations >= max_iterations) {
        // Population may not have reached target size.
    }
    // Add new organisms to population
    for (auto& organism : new_organisms) {
        population_[organism->getStats().id] = organism;
    }
    return static_cast<uint32_t>(new_organisms.size());
}

// Public, thread-safe
void Environment::applyEnvironmentalPressures() {
    std::lock_guard<std::mutex> lock(mutex_);
    apply_environmental_pressures_unlocked_();
}

// Private, NOT thread-safe
void Environment::apply_environmental_pressures_unlocked_() {
    if (population_.size() < 2) {
        return;
    }
    apply_resource_scarcity_();
    if (config_.enable_random_catastrophes) {
        apply_random_catastrophe_();
    }
    if (config_.enable_predation) {
        apply_predation_();
    }
    apply_selection_pressure_();
}

void Environment::apply_resource_scarcity_() {
    // Calculate sustainable population size based on resource abundance
    uint32_t sustainable_population = static_cast<uint32_t>(config_.max_population * config_.resource_abundance);
    if (population_.size() > sustainable_population) {
        auto to_remove = static_cast<uint32_t>(population_.size() - sustainable_population);
        // Example for random removal (resource scarcity, catastrophe, predation):
        // NOTE: For large populations, shuffling this vector could be a parallelism bottleneck.
        std::vector<uint64_t> ids;
        for (const auto& pair : population_) ids.push_back(pair.first);
        std::shuffle(ids.begin(), ids.end(), rng_);
        for (size_t i = 0; i < to_remove && i < ids.size(); ++i) {
            population_.erase(ids[i]);
        }
    }
}

void Environment::apply_random_catastrophe_() {
    // 1% chance per update
    std::uniform_real_distribution<double> chance_dist(0.0, 1.0);
    if (chance_dist(rng_) < 0.01 && !population_.empty()) {
        // Remove 10% of the population (at least 1 if population is small)
        uint32_t to_remove = std::max<uint32_t>(1, static_cast<uint32_t>(static_cast<double>(population_.size()) * 0.1));
        // Example for random removal (resource scarcity, catastrophe, predation):
        // NOTE: For large populations, shuffling this vector could be a parallelism bottleneck.
        std::vector<uint64_t> ids;
        for (const auto& pair : population_) ids.push_back(pair.first);
        std::shuffle(ids.begin(), ids.end(), rng_);
        for (size_t i = 0; i < to_remove && i < ids.size(); ++i) {
            population_.erase(ids[i]);
        }
    }
}

void Environment::apply_predation_() {
    if (population_.empty()) return;
    // Remove up to 5% of the population (at least 1 if population is small)
    uint32_t to_remove = std::max<uint32_t>(1, static_cast<uint32_t>(static_cast<double>(population_.size()) * 0.05));
    std::vector<uint64_t> candidates;
    // Build a list of organism IDs weighted by inverse fitness
    for (const auto& pair : population_) {
        if (pair.second) {
            double fitness = pair.second->getFitnessScore();
            int weight = static_cast<int>(std::max(1.0, 10.0 * (1.0 - fitness)));
            for (int w = 0; w < weight; ++w) {
                candidates.push_back(pair.first);
            }
        }
    }
    if (candidates.empty()) return;
    std::shuffle(candidates.begin(), candidates.end(), rng_);
    std::unordered_set<uint64_t> selected;
    for (uint64_t id : candidates) {
        if (selected.size() >= to_remove) break;
        selected.insert(id);
    }
    for (uint64_t id : selected) {
        population_.erase(id); // Directly erase from the map = death
        stats_.total_organisms_died++;
    }
}

void Environment::apply_selection_pressure_() {
    if (population_.empty() || config_.selection_pressure <= 0.0) {
        return;
    }
    // Cull a percentage of the lowest-fitness organisms
    uint32_t to_remove = std::max<uint32_t>(1, static_cast<uint32_t>(static_cast<double>(population_.size()) * config_.selection_pressure));
    // Sort organism IDs by fitness ascending
    std::vector<std::pair<uint64_t, double>> id_fitness;
    for (const auto& pair : population_) {
        if (pair.second) {
            id_fitness.emplace_back(pair.first, pair.second->getFitnessScore());
        }
    }
    std::sort(id_fitness.begin(), id_fitness.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });
    for (uint32_t i = 0; i < to_remove && i < id_fitness.size(); ++i) {
        population_.erase(id_fitness[i].first);
        stats_.total_organisms_died++;
    }
}

bool Environment::saveState(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        nlohmann::json state_json;
        state_json["version"] = "ENVIRONMENT_STATE_V2";
        state_json["generation"] = stats_.generation;

        nlohmann::json organisms_json = nlohmann::json::array();
        for (const auto& pair : population_) {
            if (pair.second) {
                // serialize() now returns a json object directly
                organisms_json.push_back(pair.second->serialize());
            }
        }
        state_json["organisms"] = organisms_json;
        state_json["population_size"] = organisms_json.size();

        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for saving state: {}", filename);
            return false;
        }

        file << state_json.dump(4);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception while saving environment state to {}: {}", filename, e.what());
        return false;
    }
}

bool Environment::loadState(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);

    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for loading state: {}", filename);
            return false;
        }

        nlohmann::json state_json;
        file >> state_json;

        if (!state_json.contains("version") || state_json["version"] != "ENVIRONMENT_STATE_V2") {
            spdlog::error("Invalid or missing environment state version in {}.", filename);
            return false;
        }

        population_.clear();
        stats_ = {}; // Reset stats before loading

        stats_.generation = state_json.value("generation", 0ULL);

        if (state_json.contains("organisms")) {
            for (const auto& organism_json : state_json["organisms"]) {
                auto organism = std::make_shared<Organism>(Organism::Bytecode{}, 0);
                // deserialize expects a string, so we dump the JSON object for one organism
                if (organism->deserialize(organism_json.dump())) {
                    population_[organism->getStats().id] = organism;
                } else {
                    spdlog::warn("Failed to deserialize an organism from state file.");
                }
            }
        }

        updateStats();
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::error("JSON parse error while loading state from {}: {}", filename, e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Exception while loading environment state from {}: {}", filename, e.what());
        return false;
    }
}

void Environment::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    population_.clear();
    stats_ = EnvironmentStats{};
}

std::shared_ptr<Organism> Environment::getBestOrganism() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (population_.empty()) return nullptr;
    
    auto it = std::max_element(population_.begin(), population_.end(),
        [](const auto& a, const auto& b) {
            if (!a.second) return true;
            if (!b.second) return false;
            return a.second->getFitnessScore() < b.second->getFitnessScore();
        });
    
    return (it != population_.end()) ? it->second : nullptr;
}

std::vector<Organism::Stats> Environment::getOrganismStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Organism::Stats> stats;
    stats.reserve(population_.size());
    
    for (const auto& pair : population_) {
        if (pair.second) {
            stats.push_back(pair.second->getStats());
        }
    }
    
    return stats;
}

std::vector<Environment::OrganismPtr> Environment::getTopFittest(uint32_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return select_for_reproduction_unlocked_(count);
}

Organism::Bytecode Environment::generateRandomBytecode(uint32_t size) const {
    Organism::Bytecode bytecode;
    bytecode.reserve(size);
    
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    for (uint32_t i = 0; i < size; ++i) {
        bytecode.push_back(dist(rng_));
    }
    
    return bytecode;
}

double Environment::calculateSelectionProbability(double fitness) const {
    // Simple linear selection probability based on fitness
    return std::max(0.0, std::min(1.0, fitness));
}

void Environment::updateStats() {
    stats_.population_size = static_cast<uint32_t>(population_.size());
    stats_.max_population = config_.max_population;
    
    if (!population_.empty()) {
        calculateFitnessStats();
    }
}

void Environment::applyAging() {
    (void)Clock::now(); // Suppress unused variable warning
    std::vector<uint64_t> to_remove;
    for (const auto& pair : population_) {
        if (pair.second) {
            auto age = pair.second->getAge();
            if (age.count() > config_.max_age_ms) {
                to_remove.push_back(pair.first);
            }
        }
    }
    for (uint64_t id : to_remove) {
        population_.erase(id);
        stats_.total_organisms_died++;
    }
}

void Environment::applyCompetition() {
    if (population_.size() <= 1) return;
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    std::vector<uint64_t> to_remove;
    for (const auto& pair : population_) {
        if (pair.second) {
            double fitness = pair.second->getFitnessScore();
            double death_probability = (1.0 - fitness) * config_.competition_intensity;
            if (dist(rng_) < death_probability) {
                to_remove.push_back(pair.first);
            }
        }
    }
    for (uint64_t id : to_remove) {
        population_.erase(id);
        stats_.total_organisms_died++;
    }
}

void Environment::applyCooperation() {
    // Simple cooperation: organisms with similar fitness get a bonus
    if (population_.size() <= 1) return;
    
    for (auto& pair : population_) {
        if (pair.second) {
            double current_fitness = pair.second->getFitnessScore();
            pair.second->setFitnessScore(current_fitness + config_.cooperation_bonus);
        }
    }
}

void Environment::calculateFitnessStats() {
    std::vector<double> fitness_scores;
    fitness_scores.reserve(population_.size());
    
    for (const auto& pair : population_) {
        if (pair.second) {
            fitness_scores.push_back(pair.second->getFitnessScore());
        }
    }
    
    if (!fitness_scores.empty()) {
        stats_.avg_fitness = std::accumulate(fitness_scores.begin(), fitness_scores.end(), 0.0) / static_cast<double>(fitness_scores.size());
        stats_.max_fitness = *std::max_element(fitness_scores.begin(), fitness_scores.end());
        stats_.min_fitness = *std::min_element(fitness_scores.begin(), fitness_scores.end());
        
        // Calculate variance
        double variance = 0.0;
        for (double fitness : fitness_scores) {
            const double diff = fitness - stats_.avg_fitness;
            variance += diff * diff;
        }
        stats_.fitness_variance = variance / static_cast<double>(fitness_scores.size());
    }
}

} // namespace evosim 