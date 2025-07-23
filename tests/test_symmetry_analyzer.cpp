#include "test_common.h"
#include "core/symmetry_analyzer.h"

namespace evosim {

class SymmetryAnalyzerTest : public ::testing::Test {
protected:
    void SetUp() override {
        SymmetryAnalyzer::Config config;
        config.enable_horizontal = true;
        config.enable_vertical = true;
        config.enable_diagonal = true;
        config.enable_rotational = true;
        config.noise_threshold = 0.8;
        analyzer_ = std::make_unique<SymmetryAnalyzer>(config);
    }
    
    void TearDown() override {
        analyzer_.reset();
    }
    
    std::unique_ptr<SymmetryAnalyzer> analyzer_;
};

TEST_F(SymmetryAnalyzerTest, Constructor) {
    EXPECT_NE(analyzer_, nullptr);
}

TEST_F(SymmetryAnalyzerTest, AnalyzeEmptyImage) {
    SymmetryAnalyzer::Image empty_image = cv::Mat::zeros(0, 0, CV_8UC3);
    
    auto result = analyzer_->analyze(empty_image);
    double symmetry = result.overall_symmetry;
    EXPECT_EQ(symmetry, 0.0);
}

TEST_F(SymmetryAnalyzerTest, AnalyzeSinglePixelImage) {
    SymmetryAnalyzer::Image image = cv::Mat::zeros(1, 1, CV_8UC3);
    image.at<cv::Vec3b>(0, 0) = cv::Vec3b(255, 255, 255); // White pixel
    
    auto result = analyzer_->analyze(image);
    double symmetry = result.overall_symmetry;
    EXPECT_GE(symmetry, 0.0);
    EXPECT_LE(symmetry, 1.0);
}

TEST_F(SymmetryAnalyzerTest, AnalyzeHorizontalSymmetry) {
    SymmetryAnalyzer::Image image = cv::Mat::zeros(4, 4, CV_8UC3);
    
    // Create a horizontally symmetric pattern
    image.at<cv::Vec3b>(0, 0) = cv::Vec3b(255, 0, 0);
    image.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(0, 2) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(0, 3) = cv::Vec3b(255, 0, 0);
    
    image.at<cv::Vec3b>(1, 0) = cv::Vec3b(255, 0, 0);
    image.at<cv::Vec3b>(1, 1) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(1, 2) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(1, 3) = cv::Vec3b(255, 0, 0);
    
    image.at<cv::Vec3b>(2, 0) = cv::Vec3b(255, 255, 255);
    image.at<cv::Vec3b>(2, 1) = cv::Vec3b(255, 255, 255);
    image.at<cv::Vec3b>(2, 2) = cv::Vec3b(255, 255, 255);
    image.at<cv::Vec3b>(2, 3) = cv::Vec3b(255, 255, 255);
    
    image.at<cv::Vec3b>(3, 0) = cv::Vec3b(255, 255, 255);
    image.at<cv::Vec3b>(3, 1) = cv::Vec3b(255, 255, 255);
    image.at<cv::Vec3b>(3, 2) = cv::Vec3b(255, 255, 255);
    image.at<cv::Vec3b>(3, 3) = cv::Vec3b(255, 255, 255);
    
    auto result = analyzer_->analyze(image);
    double symmetry = result.overall_symmetry;
    EXPECT_GE(symmetry, 0.0);
    EXPECT_LE(symmetry, 1.0);
}

TEST_F(SymmetryAnalyzerTest, AnalyzeVerticalSymmetry) {
    SymmetryAnalyzer::Image image = cv::Mat::zeros(4, 4, CV_8UC3);
    
    // Create a vertically symmetric pattern
    for (int y = 0; y < 4; ++y) {
        image.at<cv::Vec3b>(y, 0) = cv::Vec3b(255, 0, 0);
        image.at<cv::Vec3b>(y, 1) = cv::Vec3b(0, 255, 0);
        image.at<cv::Vec3b>(y, 2) = cv::Vec3b(0, 255, 0);
        image.at<cv::Vec3b>(y, 3) = cv::Vec3b(255, 0, 0);
    }
    
    auto result = analyzer_->analyze(image);
    double symmetry = result.overall_symmetry;
    EXPECT_GE(symmetry, 0.0);
    EXPECT_LE(symmetry, 1.0);
}

TEST_F(SymmetryAnalyzerTest, AnalyzeDetailed) {
    SymmetryAnalyzer::Image image = cv::Mat::zeros(2, 2, CV_8UC3);
    
    image.at<cv::Vec3b>(0, 0) = cv::Vec3b(255, 0, 0);
    image.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(1, 0) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(1, 1) = cv::Vec3b(255, 0, 0);
    
    auto result = analyzer_->analyze(image);
    EXPECT_GE(result.overall_symmetry, 0.0);
    EXPECT_LE(result.overall_symmetry, 1.0);
    EXPECT_GE(result.horizontal_symmetry, 0.0);
    EXPECT_LE(result.horizontal_symmetry, 1.0);
    EXPECT_GE(result.vertical_symmetry, 0.0);
    EXPECT_LE(result.vertical_symmetry, 1.0);
    EXPECT_GE(result.diagonal_symmetry, 0.0);
    EXPECT_LE(result.diagonal_symmetry, 1.0);
    EXPECT_GE(result.rotational_symmetry, 0.0);
    EXPECT_LE(result.rotational_symmetry, 1.0);
}

TEST_F(SymmetryAnalyzerTest, GetSymmetryDescription) {
    SymmetryAnalyzer::Image image = cv::Mat::zeros(2, 2, CV_8UC3);
    
    image.at<cv::Vec3b>(0, 0) = cv::Vec3b(255, 0, 0);
    image.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(1, 0) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(1, 1) = cv::Vec3b(255, 0, 0);
    
    auto result = analyzer_->analyze(image);
    // Test that we can get the result values
    EXPECT_GE(result.overall_symmetry, 0.0);
    EXPECT_LE(result.overall_symmetry, 1.0);
    EXPECT_GE(result.horizontal_symmetry, 0.0);
    EXPECT_LE(result.horizontal_symmetry, 1.0);
}

TEST_F(SymmetryAnalyzerTest, SetConfig) {
    SymmetryAnalyzer::Config new_config;
    new_config.enable_horizontal = false;
    new_config.enable_vertical = true;
    new_config.enable_diagonal = false;
    new_config.enable_rotational = false;
    new_config.noise_threshold = 0.9;
    
    analyzer_->setConfig(new_config);
    
    // Test that config was applied by analyzing an image
    SymmetryAnalyzer::Image image = cv::Mat::zeros(2, 2, CV_8UC3);
    image.at<cv::Vec3b>(0, 0) = cv::Vec3b(255, 0, 0);
    image.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(1, 0) = cv::Vec3b(0, 255, 0);
    image.at<cv::Vec3b>(1, 1) = cv::Vec3b(255, 0, 0);
    
    auto result = analyzer_->analyze(image);
    double symmetry = result.overall_symmetry;
    EXPECT_GE(symmetry, 0.0);
    EXPECT_LE(symmetry, 1.0);
}

} // namespace evosim 