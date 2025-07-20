#include "utils/random_generator.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>

namespace evosim {

// Global instance
std::unique_ptr<RandomGenerator> RandomGenerator::global_instance_;
std::mutex RandomGenerator::global_mutex_;

RandomGenerator::RandomGenerator(const Config& config)
    : config_(config)
    , seed_(0)
    , cache_index_(0) {
    initialize();
}

void RandomGenerator::setSeed(uint64_t seed) {
    std::lock_guard<std::mutex> lock(mutex_);
    seed_ = seed;
    rng_.seed(seed);
    fillCache();
}

int RandomGenerator::randomInt(int min, int max) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rng_);
}

double RandomGenerator::randomDouble(double min, double max) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uniform_real_distribution<double> dist(min, max);
    return dist(rng_);
}

bool RandomGenerator::randomBool(double probability) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::bernoulli_distribution dist(probability);
    return dist(rng_);
}

uint8_t RandomGenerator::randomByte() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    return dist(rng_);
}

std::vector<uint8_t> RandomGenerator::randomBytes(size_t count) {
    std::vector<uint8_t> bytes(count);
    std::lock_guard<std::mutex> lock(mutex_);
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    for (size_t i = 0; i < count; ++i) {
        bytes[i] = dist(rng_);
    }
    
    return bytes;
}

std::string RandomGenerator::randomString(size_t length, const std::string& charset) {
    std::string chars = charset.empty() ? 
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" : charset;
    
    std::string result;
    result.reserve(length);
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::uniform_int_distribution<size_t> dist(0, chars.length() - 1);
    
    for (size_t i = 0; i < length; ++i) {
        result += chars[dist(rng_)];
    }
    
    return result;
}

double RandomGenerator::randomNormal(double mean, double stddev) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::normal_distribution<double> dist(mean, stddev);
    return dist(rng_);
}

double RandomGenerator::randomExponential(double lambda) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::exponential_distribution<double> dist(lambda);
    return dist(rng_);
}

int RandomGenerator::randomPoisson(double mean) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::poisson_distribution<int> dist(mean);
    return dist(rng_);
}

double RandomGenerator::randomGamma(double alpha, double beta) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::gamma_distribution<double> dist(alpha, beta);
    return dist(rng_);
}

double RandomGenerator::randomLogNormal(double mean, double stddev) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::lognormal_distribution<double> dist(mean, stddev);
    return dist(rng_);
}

std::vector<int> RandomGenerator::randomPermutation(size_t size) {
    std::vector<int> perm(size);
    for (size_t i = 0; i < size; ++i) {
        perm[i] = static_cast<int>(i);
    }
    
    shuffle(perm);
    return perm;
}

std::vector<int> RandomGenerator::randomSubset(size_t size, size_t subset_size) {
    if (subset_size > size) {
        subset_size = size;
    }
    
    auto perm = randomPermutation(size);
    perm.resize(subset_size);
    return perm;
}

void RandomGenerator::setConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    initialize();
}

void RandomGenerator::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    initialize();
}

std::string RandomGenerator::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << "RandomGenerator Statistics:\n";
    oss << "  Seed: " << seed_ << "\n";
    oss << "  Cache size: " << cache_.size() << "\n";
    oss << "  Cache index: " << cache_index_ << "\n";
    oss << "  Thread safe: " << (config_.thread_safe ? "yes" : "no") << "\n";
    
    return oss.str();
}

bool RandomGenerator::saveState(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    file << "RNG_STATE_V1\n";
    file << "SEED:" << seed_ << "\n";
    file << "CACHE_SIZE:" << cache_.size() << "\n";
    
    for (double value : cache_) {
        file << value << "\n";
    }
    
    return true;
}

bool RandomGenerator::loadState(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    std::getline(file, line);
    if (line != "RNG_STATE_V1") {
        return false;
    }
    
    std::getline(file, line);
    if (line.substr(0, 5) == "SEED:") {
        seed_ = std::stoull(line.substr(5));
        rng_.seed(seed_);
    }
    
    std::getline(file, line);
    if (line.substr(0, 11) == "CACHE_SIZE:") {
        size_t cache_size = std::stoul(line.substr(11));
        cache_.clear();
        cache_.reserve(cache_size);
        
        for (size_t i = 0; i < cache_size; ++i) {
            std::getline(file, line);
            cache_.push_back(std::stod(line));
        }
    }
    
    return true;
}

RandomGenerator& RandomGenerator::getGlobal() {
    std::lock_guard<std::mutex> lock(global_mutex_);
    if (!global_instance_) {
        global_instance_ = std::make_unique<RandomGenerator>();
    }
    return *global_instance_;
}

void RandomGenerator::setGlobalSeed(uint64_t seed) {
    getGlobal().setSeed(seed);
}

void RandomGenerator::initialize() {
    if (seed_ == 0) {
        if (config_.use_time_seed) {
            seed_ = generateTimeSeed();
        }
        if (seed_ == 0 && config_.use_hardware_seed) {
            seed_ = generateHardwareSeed();
        }
        if (seed_ == 0) {
            seed_ = 12345; // Fallback seed
        }
    }
    
    rng_.seed(seed_);
    
    if (config_.enable_caching) {
        cache_.resize(config_.buffer_size);
        fillCache();
    }
}

uint64_t RandomGenerator::generateTimeSeed() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

uint64_t RandomGenerator::generateHardwareSeed() const {
    // Simple hardware-based seed using random device
    std::random_device rd;
    return rd();
}

void RandomGenerator::fillCache() {
    if (!config_.enable_caching) {
        return;
    }
    
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    for (size_t i = 0; i < cache_.size(); ++i) {
        cache_[i] = dist(rng_);
    }
    cache_index_ = 0;
}

double RandomGenerator::getCachedRandom() {
    if (cache_index_ >= cache_.size()) {
        fillCache();
    }
    return cache_[cache_index_++];
}

} // namespace evosim 