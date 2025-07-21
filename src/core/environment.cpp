#include "core/environment.h"
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include <numeric>
#include <unordered_set> // Added for apply_predation_
#include <iostream>
#include <cmath>

namespace evosim {

Environment::Environment(const Config& config)
    : config_(config)
    , vm_(BytecodeVM::Config{})
    , analyzer_(SymmetryAnalyzer::Config{})
    , rng_(std::random_device{}()) {
    initialize();
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
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // Evaluate fitness for all organisms
        for (auto& pair : population_) {
            if (pair.second) {
                double fitness = evaluateFitness(pair.second);
                pair.second->setFitnessScore(fitness);
            }
        }
        // Call unlocked helper directly
        apply_environmental_pressures_unlocked_();
        // Perform natural selection
        uint32_t deaths = performSelection();
        stats_.deaths_this_gen = deaths;
        // Perform reproduction
        uint32_t births = performReproduction();
        stats_.births_this_gen = births;
        // Update statistics
        stats_.generation++;
        stats_.last_update = Clock::now();
        updateStats();
        return true;
    } catch (const std::exception& e) {
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

double Environment::evaluateFitness(const OrganismPtr& organism) {
    if (!organism) return 0.0;
    
    // Generate image from organism's bytecode
    auto image = vm_.execute(organism->getBytecode());
    
    // Analyze symmetry
    auto result = analyzer_.analyze(image);
    return result.overall_symmetry;
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
    uint32_t initial_size = population_.size();
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
    uint32_t final_size = population_.size();
    return initial_size - final_size;
}

uint32_t Environment::performReproduction() {
    (void)population_.size(); // Suppress unused variable warning
    uint32_t target_size = std::min(config_.max_population, 
                                   std::max(config_.min_population, 
                                           static_cast<uint32_t>(std::ceil(population_.size() * 1.1))));
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
    apply_random_catastrophe_();
    apply_predation_();
    apply_selection_pressure_();
}

void Environment::apply_resource_scarcity_() {
    // Calculate sustainable population size based on resource abundance
    uint32_t sustainable_population = static_cast<uint32_t>(config_.max_population * config_.resource_abundance);
    if (population_.size() > sustainable_population) {
        uint32_t to_remove = static_cast<uint32_t>(population_.size() - sustainable_population);
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
        uint32_t to_remove = std::max<uint32_t>(1, static_cast<uint32_t>(population_.size() * 0.1));
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
    uint32_t to_remove = std::max<uint32_t>(1, static_cast<uint32_t>(population_.size() * 0.05));
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
    uint32_t to_remove = std::max<uint32_t>(1, static_cast<uint32_t>(population_.size() * config_.selection_pressure));
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
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        
        file << "ENVIRONMENT_STATE_V1\n";
        file << "GENERATION:" << stats_.generation << "\n";
        file << "POPULATION_SIZE:" << population_.size() << "\n";
        
        for (const auto& pair : population_) {
            if (pair.second) {
                file << pair.second->serialize() << "\n";
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool Environment::loadState(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        
        std::string line;
        std::getline(file, line);
        if (line != "ENVIRONMENT_STATE_V1") return false;
        
        population_.clear();
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            auto organism = std::make_shared<Organism>(Organism::Bytecode{}, 0);
            if (organism->deserialize(line)) {
                population_[organism->getStats().id] = organism;
            }
        }
        
        updateStats();
        return true;
    } catch (const std::exception& e) {
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
        stats_.avg_fitness = std::accumulate(fitness_scores.begin(), fitness_scores.end(), 0.0) / fitness_scores.size();
        stats_.max_fitness = *std::max_element(fitness_scores.begin(), fitness_scores.end());
        stats_.min_fitness = *std::min_element(fitness_scores.begin(), fitness_scores.end());
        
        // Calculate variance
        double variance = 0.0;
        for (double fitness : fitness_scores) {
            double diff = fitness - stats_.avg_fitness;
            variance += diff * diff;
        }
        stats_.fitness_variance = variance / fitness_scores.size();
    }
}

} // namespace evosim 