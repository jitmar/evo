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
#include "core/bytecode_generator.h"

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
    initialize();
}

void Environment::setVMConfig(const BytecodeVM::Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    vm_.setConfig(config);
}

void Environment::setAnalyzerConfig(const SymmetryAnalyzer::Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    analyzer_.setConfig(config);
}

void Environment::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    population_.clear();
    stats_ = EnvironmentStats{};
    
    auto vm_config = vm_.getConfig();
    BytecodeGenerator generator(vm_config.image_width, vm_config.image_height);

    // --- Create a guaranteed non-blank organism to seed the population ---
    // This ensures there is at least one organism with a visible phenotype,
    // which helps kickstart the evolutionary process.
    if (config_.initial_population > 0) {
        BytecodeGenerator::Bytecode seed_bytecode = generator.createNonBlackCirclePrimitive();
        auto seed_organism = std::make_shared<Organism>(std::move(seed_bytecode), vm_, 0);
        population_[seed_organism->getStats().id] = seed_organism;
        stats_.total_organisms_created++;
    }

    // --- Fill the rest of the population with randomly generated organisms ---
    std::uniform_int_distribution<size_t> num_primitives_dist(5, 15);
    uint32_t remaining_population = (config_.initial_population > 0) ? config_.initial_population - 1 : 0;

    for (uint32_t i = 0; i < remaining_population; ++i) {
        size_t num_primitives = num_primitives_dist(rng_);
        auto bytecode = generator.generateInitialBytecode(num_primitives);
        // We use an Organism constructor that accepts pre-made bytecode.
        auto organism = std::make_shared<Organism>(std::move(bytecode), vm_, 0);
        population_[organism->getStats().id] = organism;
        stats_.total_organisms_created++;
    }
    spdlog::info("Initialized population with {} organisms, including one guaranteed non-black seed.", config_.initial_population);
    updateStats();
}

bool Environment::update() {
    try {
        // Step 1: Get a snapshot of the current population to evaluate fitness.
        std::vector<OrganismPtr> current_population;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (population_.empty()) {
                return true; // Nothing to do.
            }
            current_population.reserve(population_.size());
            for (const auto& pair : population_) {
                current_population.push_back(pair.second);
            }
        }

        // Step 2: Perform the expensive fitness evaluation WITHOUT holding the main environment lock.
        for (const auto& organism : current_population) {
            double fitness = evaluateFitness(organism);
            organism->setFitnessScore(fitness);
        }

        // Step 3: Re-acquire the lock to apply pressures and reproduce.
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // --- Elitism: Preserve the fittest organisms ---
            const uint32_t elite_count = std::min(static_cast<uint32_t>(population_.size()), config_.elite_count);
            std::vector<OrganismPtr> elites = select_for_reproduction_unlocked_(elite_count);
            for (const auto& elite_org : elites) {
                population_.erase(elite_org->getStats().id); // Temporarily remove them from pressure
            }

            // --- Apply pressures to the non-elite population ---
            apply_environmental_pressures_unlocked_(); // Applies predation, catastrophes, etc.
            uint32_t deaths = performSelection();      // Applies aging, competition, etc.
            stats_.deaths_this_gen = deaths;

            // --- Add the elites back, safe from harm ---
            for (const auto& elite_org : elites) {
                population_[elite_org->getStats().id] = elite_org;
            }

            // --- Reproduce from the surviving population (including elites) ---
            // Create a single, sorted list of all survivors to act as the reproduction pool.
            // This is much more efficient than re-sorting inside the reproduction loop.
            std::vector<OrganismPtr> reproduction_pool = select_for_reproduction_unlocked_(static_cast<uint32_t>(population_.size()));
            uint32_t births = performReproduction(reproduction_pool);
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

Environment::ConstOrganismPtr Environment::getOrganism(uint64_t organism_id) const {
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

nlohmann::json Environment::getFullConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json full_config;
    // The to_json helpers for each config struct will be called automatically by the json library.
    full_config["environment"] = config_;
    full_config["bytecode_vm"] = vm_.getConfig();
    full_config["symmetry_analyzer"] = analyzer_.getConfig();
    return full_config;
}

double Environment::evaluateFitness(const OrganismPtr& organism) {
    if (!organism) return 0.0;
    
    // Generate image from organism's bytecode
    auto image = vm_.execute(organism->getBytecode());
    
    // --- Early Exit for Blank Images (Anti-Stagnation) ---
    // This is a crucial step to prevent evolution from getting stuck on the
    // trivial "pitch black" or other monochrome solutions. We check if the
    // image is effectively blank by looking at its standard deviation.
    cv::Scalar mean, stddev;
    cv::meanStdDev(image, mean, stddev);

    // An organism that draws nothing (or a single solid color) should have zero fitness.
    // A monochrome image has a standard deviation sum near zero.
    if ((stddev[0] + stddev[1] + stddev[2]) < 1.0) {
        return 0.0;
    }

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

uint32_t Environment::performReproduction(const std::vector<OrganismPtr>& reproduction_pool) {
    uint32_t target_size = std::min(config_.max_population,
                                   std::max(config_.min_population,
                                           static_cast<uint32_t>(std::ceil(static_cast<double>(population_.size()) * 1.1))));

    std::uniform_real_distribution<> chance_dist(0.0, 1.0);
    BytecodeGenerator generator(vm_.getConfig().image_width, vm_.getConfig().image_height);

    std::vector<OrganismPtr> new_organisms;
    const size_t max_iterations = 10 * target_size; // Safety limit
    size_t iterations = 0;
    size_t parent_idx = 0;

    while (population_.size() + new_organisms.size() < target_size && iterations < max_iterations) {
        ++iterations;

        OrganismPtr offspring;
        if (chance_dist(rng_) < config_.immigration_chance) {
            // Immigration: Create a new, random organism from scratch.
            std::uniform_int_distribution<size_t> num_primitives_dist(5, 15);
            auto bytecode = generator.generateInitialBytecode(num_primitives_dist(rng_));
            offspring = std::make_shared<Organism>(std::move(bytecode), vm_, stats_.generation);
        } else {
            // Sexual Reproduction: Crossover from two parents.
            if (reproduction_pool.size() < 2) {
                break; // Need at least two parents for crossover.
            }

            // Select two distinct parents from the pool.
            size_t p1_idx = parent_idx % reproduction_pool.size();
            size_t p2_idx = (parent_idx + 1) % reproduction_pool.size();
            if (p1_idx == p2_idx) p2_idx = (p2_idx + 1) % reproduction_pool.size();
            parent_idx++;

            const auto& parent1 = reproduction_pool[p1_idx];
            const auto& parent2 = reproduction_pool[p2_idx];

            // Perform single-point crossover on the bytecode.
            offspring = parent1->reproduceWith(parent2, vm_, config_.mutation_rate, config_.max_mutations);
        }

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
    apply_resource_scarcity_();
    if (config_.enable_random_catastrophes) {
        apply_random_catastrophe_();
    }
    if (config_.enable_predation) {
        apply_predation_();
    }
}

void Environment::remove_random_organisms_unlocked_(uint32_t count) {
    if (count == 0 || population_.empty()) {
        return;
    }

    // Ensure we don't try to remove more organisms than exist.
    count = std::min(count, static_cast<uint32_t>(population_.size()));

    // NOTE: For very large populations, creating and shuffling this vector could be a
    // performance consideration. For now, it's clear and correct.
    std::vector<uint64_t> ids;
    ids.reserve(population_.size());
    for (const auto& pair : population_) {
        ids.push_back(pair.first);
    }

    std::shuffle(ids.begin(), ids.end(), rng_);

    // Remove the first 'count' organisms from the shuffled list.
    for (uint32_t i = 0; i < count; ++i) {
        if (population_.erase(ids[i]) > 0) {
            stats_.total_organisms_died++;
        }
    }
}

void Environment::apply_resource_scarcity_() {
    // Calculate sustainable population size based on resource abundance
    uint32_t sustainable_population = static_cast<uint32_t>(config_.max_population * config_.resource_abundance);
    if (population_.size() > sustainable_population) {
        uint32_t to_remove = static_cast<uint32_t>(population_.size() - sustainable_population);
        remove_random_organisms_unlocked_(to_remove);
    }
}

void Environment::apply_random_catastrophe_() {
    // 1% chance per update
    std::uniform_real_distribution<double> chance_dist(0.0, 1.0);
    if (chance_dist(rng_) < 0.01 && !population_.empty()) {
        // Remove 10% of the population (at least 1 if population is small)
        uint32_t to_remove = std::max<uint32_t>(1, static_cast<uint32_t>(static_cast<double>(population_.size()) * 0.1));
        remove_random_organisms_unlocked_(to_remove);
    }
}

void Environment::apply_predation_() {
    // Predation requires at least two organisms to be meaningful (a predator and prey).
    if (population_.size() < 2) {
        return;
    }

    // Determine how many organisms to remove, up to 5% of the population.
    // Ensure at least one is removed, but never the entire population.
    uint32_t to_remove = std::max<uint32_t>(1, static_cast<uint32_t>(static_cast<double>(population_.size()) * 0.05));
    to_remove = std::min(to_remove, static_cast<uint32_t>(population_.size() - 1));

    std::vector<uint64_t> all_ids;
    std::vector<double> weights;
    all_ids.reserve(population_.size());
    weights.reserve(population_.size());

    // Weaker organisms (lower fitness) are more likely to be prey.
    // Their weight for removal is higher. We use inverse fitness as the weight.
    for (const auto& pair : population_) {
        if (pair.second) {
            all_ids.push_back(pair.first);
            // Add a small epsilon to avoid zero weight for organisms with perfect fitness.
            double weight = 1.0 - pair.second->getFitnessScore() + 1e-6;
            weights.push_back(weight);
        }
    }

    if (all_ids.empty()) {
        return;
    }

    // Use a discrete distribution for efficient weighted random sampling without replacement.
    std::discrete_distribution<> dist(weights.begin(), weights.end());
    std::unordered_set<uint64_t> selected_for_removal;

    // Sample until we have enough unique organisms to remove.
    // Add a safety break to prevent infinite loops if something goes wrong.
    for (size_t i = 0; i < to_remove * 5 && selected_for_removal.size() < to_remove; ++i) {
        // Cast to size_t is safe as discrete_distribution returns a value in [0, n-1].
        auto selected_index = static_cast<size_t>(dist(rng_));
        selected_for_removal.insert(all_ids.at(selected_index));
    }

    // Remove the selected organisms.
    for (uint64_t id : selected_for_removal) {
        if (population_.erase(id) > 0) {
            stats_.total_organisms_died++;
        }
    }
}

bool Environment::saveState(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);

    nlohmann::json data;
    try {
        data["version"] = "ENVIRONMENT_STATE_V4"; // Version bump for RNG state

        // Save all configurations
        data["config"] = config_;
        data["vm_config"] = vm_.getConfig();
        data["analyzer_config"] = analyzer_.getConfig();

        // Save full stats object
        data["stats"] = stats_;

        // --- Save RNG state ---
        std::stringstream rng_stream;
        rng_stream << rng_; // Attempt to serialize the RNG state
        if (rng_stream.fail()) {
            // This is unlikely but could happen if the RNG is in a bad state.
            spdlog::error("Failed to serialize RNG state to stringstream. Checkpoint will be incomplete.");
            data["rng_state"] = ""; // Save an empty string to indicate failure.
        } else {
            data["rng_state"] = rng_stream.str();
        }

        nlohmann::json& organisms_json = data["organisms"] = nlohmann::json::array();
        for (const auto& pair : population_) {
            if (pair.second) {
                organisms_json.push_back(pair.second->serialize());
            }
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for saving state: {}", filename);
            return false;
        }

        file << data.dump(4);

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Exception while saving environment state to {}: {}", filename, e.what());
        return false;
    }
}

bool Environment::loadState(const std::string& filename) {
    spdlog::debug("Environment::loadState: Starting state load from '{}'...", filename);
    nlohmann::json data;
    try {
        // --- Robust File Reading: Read to string first to avoid stream parsing hangs ---
        spdlog::debug("Environment::loadState: Opening and reading file content...");
        std::ifstream f(filename);
        if (!f.is_open()) {
            spdlog::error("Failed to open checkpoint file: {}", filename);
            return false;
        }
        std::string file_contents((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();

        spdlog::debug("Environment::loadState: File read complete ({} bytes). Parsing JSON...", file_contents.length());
        data = nlohmann::json::parse(file_contents);
        spdlog::debug("Environment::loadState: JSON parsed successfully.");

    } catch (const std::exception& e) {
        spdlog::error("Failed to read or parse checkpoint file '{}': {}", filename, e.what());
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::string version = data.value("version", "UNKNOWN");
        if (version != "ENVIRONMENT_STATE_V4" && version != "ENVIRONMENT_STATE_V3") {
            spdlog::error("Invalid or unsupported environment state version in {}. Expected V3 or V4, got {}.", filename, version);
            return false;
        }

        // Clear existing state
        population_.clear();
        stats_ = {};

        // Load all configurations
        config_ = data.at("config").get<Config>();
        vm_.setConfig(data.at("vm_config").get<BytecodeVM::Config>());
        analyzer_.setConfig(data.at("analyzer_config").get<SymmetryAnalyzer::Config>());

        // Load stats
        stats_ = data.at("stats").get<EnvironmentStats>();

        // --- Safely load RNG state (FIX for hang) ---
        if (data.contains("rng_state") && !data.at("rng_state").is_null()) {
            std::string rng_state_str = data.at("rng_state").get<std::string>();
            if (!rng_state_str.empty()) {
                std::stringstream rng_stream(rng_state_str);
                rng_stream >> rng_; // Attempt to restore RNG state
                if (rng_stream.fail()) {
                    // If the stream fails (e.g., due to corrupt data), it can hang.
                    // We must check the state and re-seed as a safe fallback.
                    spdlog::warn("Failed to restore RNG state from checkpoint '{}'. State may be corrupt. Re-seeding RNG.", filename);
                    rng_.seed(std::random_device{}());
                }
            } else {
                 spdlog::warn("Checkpoint file '{}' contains an empty RNG state. Seeding new RNG.", filename);
                 rng_.seed(std::random_device{}());
            }
        } else {
            spdlog::warn("Loading older checkpoint '{}' without RNG state. Restart will not be fully reproducible. Seeding new RNG.", filename);
            rng_.seed(std::random_device{}());
        }

        // Load organisms
        if (data.contains("organisms")) {
            for (const auto& organism_json : data.at("organisms")) {
                auto organism = std::make_shared<Organism>(Organism::Bytecode{}, vm_, 0);
                // deserialize expects a string, so we dump the JSON object for one organism
                if (organism->deserialize(organism_json.dump(), vm_)) {
                    population_[organism->getStats().id] = organism;
                } else {
                    spdlog::warn("Failed to deserialize an organism from state file.");
                }
            }
        }

        updateStats();
        return true;
    } catch (const nlohmann::json::exception& e) {
        spdlog::error("JSON error while processing state from {}: {}", filename, e.what());
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

Environment::ConstOrganismPtr Environment::getBestOrganism() const {
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

std::vector<Environment::ConstOrganismPtr> Environment::getTopFittest(uint32_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
	auto fittest_non_const = select_for_reproduction_unlocked_(count);
	// Construct a new vector of const pointers from the vector of non-const pointers.
	return {fittest_non_const.begin(), fittest_non_const.end()};
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
