#pragma once

#include <random>
#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>

namespace evosim {

/**
 * @brief Thread-safe random number generator
 * 
 * Provides various random number generation capabilities with
 * thread safety and configurable distributions.
 */
class RandomGenerator {
public:
    /**
     * @brief Random number distribution types
     */
    enum class DistributionType {
        UNIFORM_INT,        ///< Uniform integer distribution
        UNIFORM_REAL,       ///< Uniform real distribution
        NORMAL,             ///< Normal (Gaussian) distribution
        EXPONENTIAL,        ///< Exponential distribution
        POISSON,            ///< Poisson distribution
        BERNOULLI,          ///< Bernoulli distribution
        GEOMETRIC,          ///< Geometric distribution
        BINOMIAL,           ///< Binomial distribution
        GAMMA,              ///< Gamma distribution
        WEIBULL,            ///< Weibull distribution
        CHI_SQUARED,        ///< Chi-squared distribution
        STUDENT_T,          ///< Student's t-distribution
        FISHER_F,           ///< Fisher's F-distribution
        CAUCHY,             ///< Cauchy distribution
        LOG_NORMAL          ///< Log-normal distribution
    };

    /**
     * @brief Random generator configuration
     */
    struct Config {
        uint64_t seed;                 ///< Random seed (0 for auto)
        bool use_time_seed;            ///< Use time-based seed if seed is 0
        bool use_hardware_seed;        ///< Use hardware entropy if available
        bool thread_safe;              ///< Enable thread safety
        uint32_t buffer_size;          ///< Random number buffer size
        bool enable_caching;           ///< Enable random number caching
        
        Config() : seed(0), use_time_seed(true), use_hardware_seed(true),
                   thread_safe(true), buffer_size(1024), enable_caching(true) {}
    };

    /**
     * @brief Constructor
     * @param config Generator configuration
     */
    explicit RandomGenerator(const Config& config = Config());

    /**
     * @brief Destructor
     */
    ~RandomGenerator() = default;

    /**
     * @brief Set random seed
     * @param seed Random seed
     */
    void setSeed(uint64_t seed);

    /**
     * @brief Get current seed
     * @return Current seed
     */
    uint64_t getSeed() const { return seed_; }

    /**
     * @brief Generate random integer
     * @param min Minimum value (inclusive)
     * @param max Maximum value (inclusive)
     * @return Random integer
     */
    int randomInt(int min, int max);

    /**
     * @brief Generate random double
     * @param min Minimum value (inclusive)
     * @param max Maximum value (inclusive)
     * @return Random double
     */
    double randomDouble(double min, double max);

    /**
     * @brief Generate random boolean
     * @param probability Probability of true (0.0 to 1.0)
     * @return Random boolean
     */
    bool randomBool(double probability = 0.5);

    /**
     * @brief Generate random byte
     * @return Random byte (0-255)
     */
    uint8_t randomByte();

    /**
     * @brief Generate random bytes
     * @param count Number of bytes to generate
     * @return Vector of random bytes
     */
    std::vector<uint8_t> randomBytes(size_t count);

    /**
     * @brief Generate random string
     * @param length String length
     * @param charset Character set (empty for alphanumeric)
     * @return Random string
     */
    std::string randomString(size_t length, const std::string& charset = "");

    /**
     * @brief Generate random value from distribution
     * @param type Distribution type
     * @param params Distribution parameters
     * @return Random value
     */
    double randomFromDistribution(DistributionType type, const std::vector<double>& params = {});

    /**
     * @brief Generate random value from normal distribution
     * @param mean Distribution mean
     * @param stddev Distribution standard deviation
     * @return Random value
     */
    double randomNormal(double mean = 0.0, double stddev = 1.0);

    /**
     * @brief Generate random value from exponential distribution
     * @param lambda Rate parameter
     * @return Random value
     */
    double randomExponential(double lambda = 1.0);

    /**
     * @brief Generate random value from Poisson distribution
     * @param mean Distribution mean
     * @return Random value
     */
    int randomPoisson(double mean = 1.0);

    /**
     * @brief Generate random value from gamma distribution
     * @param alpha Shape parameter
     * @param beta Scale parameter
     * @return Random value
     */
    double randomGamma(double alpha = 1.0, double beta = 1.0);

    /**
     * @brief Generate random value from log-normal distribution
     * @param mean Distribution mean
     * @param stddev Distribution standard deviation
     * @return Random value
     */
    double randomLogNormal(double mean = 0.0, double stddev = 1.0);

    /**
     * @brief Shuffle vector
     * @param vec Vector to shuffle
     */
    template<typename T>
    void shuffle(std::vector<T>& vec) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::shuffle(vec.begin(), vec.end(), rng_);
    }

    /**
     * @brief Choose random element from vector
     * @param vec Vector to choose from
     * @return Random element
     */
    template<typename T>
    T choose(const std::vector<T>& vec) {
        if (vec.empty()) {
            return T{};
        }
        return vec[randomInt(0, static_cast<int>(vec.size()) - 1)];
    }

    /**
     * @brief Choose random element with weights
     * @param elements Elements to choose from
     * @param weights Weights for each element
     * @return Random element
     */
    template<typename T>
    T chooseWeighted(const std::vector<T>& elements, const std::vector<double>& weights) {
        if (elements.empty() || elements.size() != weights.size()) {
            return T{};
        }

        double total_weight = 0.0;
        for (double weight : weights) {
            total_weight += weight;
        }

        double random_value = randomDouble(0.0, total_weight);
        double cumulative_weight = 0.0;

        for (size_t i = 0; i < elements.size(); ++i) {
            cumulative_weight += weights[i];
            if (random_value <= cumulative_weight) {
                return elements[i];
            }
        }

        return elements.back();
    }

    /**
     * @brief Generate random permutation
     * @param size Size of permutation
     * @return Random permutation
     */
    std::vector<int> randomPermutation(size_t size);

    /**
     * @brief Generate random subset
     * @param size Total size
     * @param subset_size Subset size
     * @return Random subset indices
     */
    std::vector<int> randomSubset(size_t size, size_t subset_size);

    /**
     * @brief Get random generator configuration
     * @return Current configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * @brief Set random generator configuration
     * @param config New configuration
     */
    void setConfig(const Config& config);

    /**
     * @brief Reset random generator
     */
    void reset();

    /**
     * @brief Get random generator statistics
     * @return Statistics string
     */
    std::string getStatistics() const;

    /**
     * @brief Save random generator state
     * @param filename Output filename
     * @return True if successful
     */
    bool saveState(const std::string& filename) const;

    /**
     * @brief Load random generator state
     * @param filename Input filename
     * @return True if successful
     */
    bool loadState(const std::string& filename);

    /**
     * @brief Get global random generator instance
     * @return Global random generator
     */
    static RandomGenerator& getGlobal();

    /**
     * @brief Set global random generator seed
     * @param seed Random seed
     */
    static void setGlobalSeed(uint64_t seed);

private:
    Config config_;                     ///< Generator configuration
    uint64_t seed_;                     ///< Current seed
    std::mt19937 rng_;                  ///< Mersenne Twister random generator
    mutable std::mutex mutex_;          ///< Thread safety mutex
    std::vector<double> cache_;         ///< Random number cache
    size_t cache_index_;                ///< Cache index

    static std::unique_ptr<RandomGenerator> global_instance_; ///< Global instance
    static std::mutex global_mutex_;     ///< Global instance mutex

    /**
     * @brief Initialize random generator
     */
    void initialize();

    /**
     * @brief Generate seed from time
     * @return Time-based seed
     */
    uint64_t generateTimeSeed() const;

    /**
     * @brief Generate seed from hardware
     * @return Hardware-based seed
     */
    uint64_t generateHardwareSeed() const;

    /**
     * @brief Fill random number cache
     */
    void fillCache();

    /**
     * @brief Get cached random number
     * @return Cached random number
     */
    double getCachedRandom();

    /**
     * @brief Create distribution
     * @param type Distribution type
     * @param params Distribution parameters
     * @return Distribution pointer
     */
    std::unique_ptr<std::uniform_real_distribution<double>> createDistribution(
        DistributionType type, const std::vector<double>& params = {}) const;
};

} // namespace evosim 