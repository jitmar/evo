#include "gtest/gtest.h"
#include "core/bytecode_vm.h"

using namespace evosim;

// Test fixture for BytecodeVM tests
class BytecodeVMTest : public ::testing::Test {
protected:
    BytecodeVM::Config config;
    BytecodeVM vm;

    BytecodeVMTest() : vm(config) {
        config.image_width = 50;
        config.image_height = 50;
        vm.setConfig(config);
    }
};

TEST_F(BytecodeVMTest, ExecutesRGBColorOpcodesCorrectly) {
    // Bytecode to set a specific RGB color and draw a pixel
    BytecodeVM::Bytecode bytecode = {
        // Set color to R=100, G=150, B=200
        static_cast<uint8_t>(BytecodeVM::Opcode::PUSH), 100, // Push Red
        static_cast<uint8_t>(BytecodeVM::Opcode::SET_COLOR_R),
        static_cast<uint8_t>(BytecodeVM::Opcode::PUSH), 150, // Push Green
        static_cast<uint8_t>(BytecodeVM::Opcode::SET_COLOR_G),
        static_cast<uint8_t>(BytecodeVM::Opcode::PUSH), 200, // Push Blue
        static_cast<uint8_t>(BytecodeVM::Opcode::SET_COLOR_B),

        // Set position to (x=10, y=20)
        static_cast<uint8_t>(BytecodeVM::Opcode::SET_X), 10,
        static_cast<uint8_t>(BytecodeVM::Opcode::SET_Y), 20,

        // Draw the pixel
        static_cast<uint8_t>(BytecodeVM::Opcode::DRAW_PIXEL),

        // Halt execution
        static_cast<uint8_t>(BytecodeVM::Opcode::HALT)
    };

    // Execute the bytecode
    BytecodeVM::Image result_image = vm.execute(bytecode);

    // --- Verification ---
    ASSERT_FALSE(result_image.empty());

    // Get the color of the pixel at (x=10, y=20)
    // Note: OpenCV's at() uses (row, col) which corresponds to (y, x)
    cv::Vec3b pixel_color = result_image.at<cv::Vec3b>(20, 10);

    // Assert that the pixel has the correct color (OpenCV uses BGR order)
    EXPECT_EQ(pixel_color[0], 200); // Blue
    EXPECT_EQ(pixel_color[1], 150); // Green
    EXPECT_EQ(pixel_color[2], 100); // Red
}