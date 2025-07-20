#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <memory>

namespace evosim {

/**
 * @brief Analyzes images for symmetry patterns and calculates fitness scores
 * 
 * The symmetry analyzer evaluates how symmetric an image is across different
 * axes and patterns, providing a fitness score for evolutionary selection.
 */
class SymmetryAnalyzer {
public:
    using Image = cv::Mat;

    /**
     * @brief Symmetry analysis results
     */
    struct SymmetryResult {
        double horizontal_symmetry;     ///< Horizontal symmetry score (0-1)
        double vertical_symmetry;       ///< Vertical symmetry score (0-1)
        double diagonal_symmetry;       ///< Diagonal symmetry score (0-1)
        double rotational_symmetry;     ///< Rotational symmetry score (0-1)
        double overall_symmetry;        ///< Combined symmetry score (0-1)
        double complexity_score;        ///< Pattern complexity score (0-1)
        double fitness_score;           ///< Final fitness score (0-1)
        std::vector<double> symmetry_histogram; ///< Symmetry distribution
    };

    /**
     * @brief Analyzer configuration
     */
    struct Config {
        bool enable_horizontal;      ///< Enable horizontal symmetry analysis
        bool enable_vertical;        ///< Enable vertical symmetry analysis
        bool enable_diagonal;        ///< Enable diagonal symmetry analysis
        bool enable_rotational;      ///< Enable rotational symmetry analysis
        bool enable_complexity;      ///< Enable complexity analysis
        double horizontal_weight;    ///< Weight for horizontal symmetry
        double vertical_weight;      ///< Weight for vertical symmetry
        double diagonal_weight;      ///< Weight for diagonal symmetry
        double rotational_weight;    ///< Weight for rotational symmetry
        double complexity_weight;    ///< Weight for complexity
        uint32_t histogram_bins;     ///< Number of histogram bins
        double noise_threshold;      ///< Noise threshold for analysis
        bool normalize_scores;       ///< Normalize scores to 0-1 range
        
        Config() : enable_horizontal(true), enable_vertical(true), enable_diagonal(true),
                   enable_rotational(true), enable_complexity(true), horizontal_weight(0.25),
                   vertical_weight(0.25), diagonal_weight(0.20), rotational_weight(0.20),
                   complexity_weight(0.10), histogram_bins(64), noise_threshold(0.05),
                   normalize_scores(true) {}
    };

    /**
     * @brief Constructor
     * @param config Analyzer configuration
     */
    explicit SymmetryAnalyzer(const Config& config = Config());

    /**
     * @brief Destructor
     */
    ~SymmetryAnalyzer() = default;

    /**
     * @brief Analyze image for symmetry
     * @param image Image to analyze
     * @return Symmetry analysis results
     */
    SymmetryResult analyze(const Image& image);

    /**
     * @brief Analyze image with custom configuration
     * @param image Image to analyze
     * @param config Custom configuration
     * @return Symmetry analysis results
     */
    SymmetryResult analyze(const Image& image, const Config& config);

    /**
     * @brief Calculate horizontal symmetry
     * @param image Image to analyze
     * @return Horizontal symmetry score (0-1)
     */
    double calculateHorizontalSymmetry(const Image& image) const;

    /**
     * @brief Calculate vertical symmetry
     * @param image Image to analyze
     * @return Vertical symmetry score (0-1)
     */
    double calculateVerticalSymmetry(const Image& image) const;

    /**
     * @brief Calculate diagonal symmetry
     * @param image Image to analyze
     * @return Diagonal symmetry score (0-1)
     */
    double calculateDiagonalSymmetry(const Image& image) const;

    /**
     * @brief Calculate rotational symmetry
     * @param image Image to analyze
     * @return Rotational symmetry score (0-1)
     */
    double calculateRotationalSymmetry(const Image& image) const;

    /**
     * @brief Calculate pattern complexity
     * @param image Image to analyze
     * @return Complexity score (0-1)
     */
    double calculateComplexity(const Image& image) const;

    /**
     * @brief Calculate overall fitness score
     * @param result Symmetry analysis result
     * @return Fitness score (0-1)
     */
    double calculateFitnessScore(const SymmetryResult& result) const;

    /**
     * @brief Get symmetry description as string
     * @param result Symmetry analysis result
     * @return Formatted description string
     */
    std::string getSymmetryDescription(const SymmetryResult& result) const;

    /**
     * @brief Set analyzer configuration
     * @param config New configuration
     */
    void setConfig(const Config& config) { config_ = config; }

    /**
     * @brief Get analyzer configuration
     * @return Current configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * @brief Generate symmetry visualization
     * @param image Original image
     * @param result Symmetry analysis result
     * @return Visualization image
     */
    Image generateVisualization(const Image& image, const SymmetryResult& result) const;

    /**
     * @brief Save analysis report
     * @param result Symmetry analysis result
     * @param filename Output filename
     * @return True if successful
     */
    bool saveReport(const SymmetryResult& result, const std::string& filename) const;

private:
    Config config_;                     ///< Analyzer configuration

    /**
     * @brief Calculate symmetry between two image regions
     * @param region1 First region
     * @param region2 Second region
     * @return Symmetry score (0-1)
     */
    double calculateRegionSymmetry(const Image& region1, const Image& region2) const;

    /**
     * @brief Calculate histogram-based symmetry
     * @param image Image to analyze
     * @param axis Symmetry axis (0=horizontal, 1=vertical)
     * @return Symmetry score (0-1)
     */
    double calculateHistogramSymmetry(const Image& image, int axis) const;

    /**
     * @brief Calculate gradient-based symmetry
     * @param image Image to analyze
     * @param axis Symmetry axis (0=horizontal, 1=vertical)
     * @return Symmetry score (0-1)
     */
    double calculateGradientSymmetry(const Image& image, int axis) const;

    /**
     * @brief Calculate edge-based symmetry
     * @param image Image to analyze
     * @param axis Symmetry axis (0=horizontal, 1=vertical)
     * @return Symmetry score (0-1)
     */
    double calculateEdgeSymmetry(const Image& image, int axis) const;

    /**
     * @brief Normalize image for analysis
     * @param image Input image
     * @return Normalized image
     */
    Image normalizeImage(const Image& image) const;

    /**
     * @brief Apply noise reduction
     * @param image Input image
     * @return Denoised image
     */
    Image denoiseImage(const Image& image) const;

    /**
     * @brief Calculate image entropy
     * @param image Input image
     * @return Entropy value
     */
    double calculateEntropy(const Image& image) const;

    /**
     * @brief Calculate image variance
     * @param image Input image
     * @return Variance value
     */
    double calculateVariance(const Image& image) const;
};

} // namespace evosim 