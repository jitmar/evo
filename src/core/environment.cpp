#include "core/environment.h"
#include <algorithm>
#include <random>
#include <fstream>
#include <sstream>
#include <numeric>

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
        population_.push_back(organism);
        stats_.total_organisms_created++;
    }
    
    updateStats();
}

bool Environment::update() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Evaluate fitness for all organisms
        for (auto& organism : population_) {
            if (organism) {
                double fitness = evaluateFitness(organism);
                organism->setFitnessScore(fitness);
            }
        }
        
        // Apply environmental pressures
        applyEnvironmentalPressures();
        
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
    
    population_.push_back(organism);
    stats_.total_organisms_created++;
    updateStats();
    
    return true;
}

bool Environment::removeOrganism(uint64_t organism_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = std::find_if(population_.begin(), population_.end(),
        [organism_id](const OrganismPtr& org) {
            return org && org->getStats().id == organism_id;
        });
    
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
    
    auto it = std::find_if(population_.begin(), population_.end(),
        [organism_id](const OrganismPtr& org) {
            return org && org->getStats().id == organism_id;
        });
    
    return (it != population_.end()) ? *it : nullptr;
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
    
    if (population_.empty()) {
        return {};
    }
    
    // Sort by fitness (descending)
    std::vector<std::shared_ptr<Organism>> sorted_population = population_;
    std::sort(sorted_population.begin(), sorted_population.end(),
        [](const std::shared_ptr<Organism>& a, const std::shared_ptr<Organism>& b) {
            if (!a) return false;
            if (!b) return true;
            return a->getFitnessScore() > b->getFitnessScore();
        });
    
    // Select top organisms
    count = std::min(count, static_cast<uint32_t>(sorted_population.size()));
    return std::vector<std::shared_ptr<Organism>>(sorted_population.begin(), 
                                   sorted_population.begin() + count);
}

uint32_t Environment::performSelection() {
    uint32_t initial_size = population_.size();
    
            // Remove dead organisms
        population_.erase(
            std::remove_if(population_.begin(), population_.end(),
                [](const std::shared_ptr<Organism>& org) {
                    return !org || !org->isAlive();
                }),
            population_.end()
        );
    
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
                                           static_cast<uint32_t>(population_.size() * 1.1)));
    
    std::vector<OrganismPtr> new_organisms;
    
    while (population_.size() + new_organisms.size() < target_size) {
        // Select parent for reproduction
        auto parents = selectForReproduction(1);
        if (parents.empty()) break;
        
        auto parent = parents[0];
        if (!parent) continue;
        
        // Create offspring with mutations
        auto offspring = parent->replicate(config_.mutation_rate, config_.max_mutations);
        if (offspring) {
            new_organisms.push_back(offspring);
            stats_.total_organisms_created++;
        }
    }
    
    // Add new organisms to population
    population_.insert(population_.end(), new_organisms.begin(), new_organisms.end());
    
    return static_cast<uint32_t>(new_organisms.size());
}

void Environment::applyEnvironmentalPressures() {
    // This is a placeholder for environmental pressure application
    // In a full implementation, this would apply various pressures
    // like resource scarcity, predation, etc.
}

bool Environment::saveState(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        
        file << "ENVIRONMENT_STATE_V1\n";
        file << "GENERATION:" << stats_.generation << "\n";
        file << "POPULATION_SIZE:" << population_.size() << "\n";
        
        for (const auto& organism : population_) {
            if (organism) {
                file << organism->serialize() << "\n";
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
                population_.push_back(organism);
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
        [](const std::shared_ptr<Organism>& a, const std::shared_ptr<Organism>& b) {
            if (!a) return true;
            if (!b) return false;
            return a->getFitnessScore() < b->getFitnessScore();
        });
    
    return (it != population_.end()) ? *it : nullptr;
}

std::vector<Organism::Stats> Environment::getOrganismStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Organism::Stats> stats;
    stats.reserve(population_.size());
    
    for (const auto& organism : population_) {
        if (organism) {
            stats.push_back(organism->getStats());
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
    
    for (auto& organism : population_) {
        if (organism) {
            auto age = organism->getAge();
            if (age.count() > config_.max_age_ms) {
                organism->die();
            }
        }
    }
}

void Environment::applyCompetition() {
    if (population_.size() <= 1) return;
    
    // Simple competition: organisms with lower fitness have a chance to die
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    for (auto& organism : population_) {
        if (organism && organism->isAlive()) {
            double fitness = organism->getFitnessScore();
            double death_probability = (1.0 - fitness) * config_.competition_intensity;
            
            if (dist(rng_) < death_probability) {
                organism->die();
            }
        }
    }
}

void Environment::applyCooperation() {
    // Simple cooperation: organisms with similar fitness get a bonus
    if (population_.size() <= 1) return;
    
    for (auto& organism : population_) {
        if (organism && organism->isAlive()) {
            double current_fitness = organism->getFitnessScore();
            organism->setFitnessScore(current_fitness + config_.cooperation_bonus);
        }
    }
}

void Environment::calculateFitnessStats() {
    std::vector<double> fitness_scores;
    fitness_scores.reserve(population_.size());
    
    for (const auto& organism : population_) {
        if (organism) {
            fitness_scores.push_back(organism->getFitnessScore());
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

void Environment::sortPopulationByFitness() {
    std::sort(population_.begin(), population_.end(),
        [](const std::shared_ptr<Organism>& a, const std::shared_ptr<Organism>& b) {
            if (!a) return false;
            if (!b) return true;
            return a->getFitnessScore() > b->getFitnessScore();
        });
}

} // namespace evosim 