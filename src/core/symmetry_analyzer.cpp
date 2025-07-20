#include "core/symmetry_analyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iomanip>
#include <sstream>

namespace evosim {

SymmetryAnalyzer::SymmetryAnalyzer(const Config& config)
    : config_(config) {
}



SymmetryAnalyzer::SymmetryResult SymmetryAnalyzer::analyze(const Image& image) {
    SymmetryResult result;
    
    if (image.empty()) {
        return result;
    }
    
    result.horizontal_symmetry = calculateHorizontalSymmetry(image);
    result.vertical_symmetry = calculateVerticalSymmetry(image);
    result.diagonal_symmetry = calculateDiagonalSymmetry(image);
    result.rotational_symmetry = calculateRotationalSymmetry(image);
    result.complexity_score = calculateComplexity(image);
    
    // Calculate overall symmetry
    std::vector<double> symmetries = {
        result.horizontal_symmetry,
        result.vertical_symmetry,
        result.diagonal_symmetry,
        result.rotational_symmetry
    };
    
    result.overall_symmetry = std::accumulate(symmetries.begin(), symmetries.end(), 0.0) / symmetries.size();
    
    // Calculate fitness score
    result.fitness_score = calculateFitnessScore(result);
    
    return result;
}

std::string SymmetryAnalyzer::getSymmetryDescription(const SymmetryResult& result) const {
    std::ostringstream oss;
    
    oss << "Symmetry Analysis:\n";
    oss << "  Overall: " << std::fixed << std::setprecision(3) << result.overall_symmetry << "\n";
    oss << "  Horizontal: " << result.horizontal_symmetry << "\n";
    oss << "  Vertical: " << result.vertical_symmetry << "\n";
    oss << "  Diagonal: " << result.diagonal_symmetry << "\n";
    oss << "  Rotational: " << result.rotational_symmetry << "\n";
    oss << "  Complexity: " << result.complexity_score << "\n";
    oss << "  Fitness: " << result.fitness_score << "\n";
    
    return oss.str();
}

double SymmetryAnalyzer::calculateFitnessScore(const SymmetryResult& result) const {
    double fitness = 0.0;
    
    if (config_.enable_horizontal) {
        fitness += result.horizontal_symmetry * config_.horizontal_weight;
    }
    if (config_.enable_vertical) {
        fitness += result.vertical_symmetry * config_.vertical_weight;
    }
    if (config_.enable_diagonal) {
        fitness += result.diagonal_symmetry * config_.diagonal_weight;
    }
    if (config_.enable_rotational) {
        fitness += result.rotational_symmetry * config_.rotational_weight;
    }
    if (config_.enable_complexity) {
        fitness += result.complexity_score * config_.complexity_weight;
    }
    
    return std::max(0.0, std::min(1.0, fitness));
}

double SymmetryAnalyzer::calculateHorizontalSymmetry(const Image& image) const {
    if (image.rows < 2) return 0.0;
    
    double total_diff = 0.0;
    int comparisons = 0;
    
    for (int y = 0; y < image.rows / 2; ++y) {
        int mirror_y = image.rows - 1 - y;
        
        for (int x = 0; x < image.cols; ++x) {
            cv::Vec3b pixel1 = image.at<cv::Vec3b>(y, x);
            cv::Vec3b pixel2 = image.at<cv::Vec3b>(mirror_y, x);
            
            for (int c = 0; c < 3; ++c) { // RGB channels
                int diff = std::abs(static_cast<int>(pixel1[c]) - static_cast<int>(pixel2[c]));
                total_diff += diff;
                comparisons++;
            }
        }
    }
    
    if (comparisons == 0) return 0.0;
    
    // Convert to similarity (0 = no symmetry, 1 = perfect symmetry)
    double avg_diff = total_diff / comparisons;
    return std::max(0.0, 1.0 - (avg_diff / 255.0));
}

double SymmetryAnalyzer::calculateVerticalSymmetry(const Image& image) const {
    if (image.cols < 2) return 0.0;
    
    double total_diff = 0.0;
    int comparisons = 0;
    
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols / 2; ++x) {
            int mirror_x = image.cols - 1 - x;
            
            cv::Vec3b pixel1 = image.at<cv::Vec3b>(y, x);
            cv::Vec3b pixel2 = image.at<cv::Vec3b>(y, mirror_x);
            
            for (int c = 0; c < 3; ++c) { // RGB channels
                int diff = std::abs(static_cast<int>(pixel1[c]) - static_cast<int>(pixel2[c]));
                total_diff += diff;
                comparisons++;
            }
        }
    }
    
    if (comparisons == 0) return 0.0;
    
    double avg_diff = total_diff / comparisons;
    return std::max(0.0, 1.0 - (avg_diff / 255.0));
}

double SymmetryAnalyzer::calculateDiagonalSymmetry(const Image& image) const {
    if (image.cols < 2 || image.rows < 2) return 0.0;
    
    double total_diff = 0.0;
    int comparisons = 0;
    
    int min_dim = std::min(image.cols, image.rows);
    
    for (int i = 0; i < min_dim; ++i) {
        for (int j = i + 1; j < min_dim; ++j) {
            cv::Vec3b pixel1 = image.at<cv::Vec3b>(i, j);
            cv::Vec3b pixel2 = image.at<cv::Vec3b>(j, i);
            
            for (int c = 0; c < 3; ++c) { // RGB channels
                int diff = std::abs(static_cast<int>(pixel1[c]) - static_cast<int>(pixel2[c]));
                total_diff += diff;
                comparisons++;
            }
        }
    }
    
    if (comparisons == 0) return 0.0;
    
    double avg_diff = total_diff / comparisons;
    return std::max(0.0, 1.0 - (avg_diff / 255.0));
}

double SymmetryAnalyzer::calculateRotationalSymmetry(const Image& image) const {
    if (image.cols < 2 || image.rows < 2) return 0.0;
    
    // Simple implementation: compare quadrants
    double total_diff = 0.0;
    int comparisons = 0;
    
    for (int y = 0; y < image.rows / 2; ++y) {
        for (int x = 0; x < image.cols / 2; ++x) {
            cv::Vec3b pixel1 = image.at<cv::Vec3b>(y, x);
            cv::Vec3b pixel2 = image.at<cv::Vec3b>(image.rows - 1 - y, image.cols - 1 - x);
            
            for (int c = 0; c < 3; ++c) { // RGB channels
                int diff = std::abs(static_cast<int>(pixel1[c]) - static_cast<int>(pixel2[c]));
                total_diff += diff;
                comparisons++;
            }
        }
    }
    
    if (comparisons == 0) return 0.0;
    
    double avg_diff = total_diff / comparisons;
    return std::max(0.0, 1.0 - (avg_diff / 255.0));
}

double SymmetryAnalyzer::calculateComplexity(const Image& image) const {
    if (image.empty()) return 0.0;
    
    // Simple complexity measure based on edge detection
    cv::Mat gray, edges;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::Canny(gray, edges, 50, 150);
    
    // Count edge pixels
    int edge_pixels = cv::countNonZero(edges);
    int total_pixels = image.rows * image.cols;
    
    if (total_pixels == 0) return 0.0;
    
    // Normalize to 0-1 range
    double complexity = static_cast<double>(edge_pixels) / total_pixels;
    return std::min(1.0, complexity * 10.0); // Scale up for better visibility
}


} // namespace evosim 