#pragma once

#include <vector>
#include <cstdint>
#include <random>
#include <functional>
#include "core/opcodes.h"

namespace evosim {

/**
 * @brief Generates structured bytecode for creating initial organisms.
 *
 * This class creates meaningful sequences of bytecode (primitives) that
 * can be combined to form the initial "genome" of an organism. This is
 * more effective than purely random bytecode, as it provides a better
 * starting point for evolution.
 */
class BytecodeGenerator {
public:
    using Bytecode = std::vector<uint8_t>;

    BytecodeGenerator(uint32_t image_width, uint32_t image_height, double composite_chance = 0.25);
    Bytecode generateInitialBytecode(size_t num_primitives);

    // --- Random Primitive Generators (for initial population) ---
    Bytecode createNonBlackCirclePrimitive();
    Bytecode createLinePrimitive();
    Bytecode createBezierCurvePrimitive();
    Bytecode createTrianglePrimitive();
    Bytecode createNonBlackRectanglePrimitive();

    // --- Composite Generators (combining primitives) ---
    Bytecode createStickFigureComposite();

    // --- Parameterized Primitive Builders (for composites) ---
    Bytecode createCircle(uint8_t x, uint8_t y, uint8_t radius);
    Bytecode createRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
    Bytecode createLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
    Bytecode createTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3);
    Bytecode createBezierCurve(uint8_t x0, uint8_t y0, uint8_t cx, uint8_t cy, uint8_t ex, uint8_t ey);

private:
    uint32_t image_width_;
    uint32_t image_height_;
    double composite_chance_;
    std::mt19937 rng_;
    std::vector<std::function<Bytecode()>> primitive_generators_;
    std::vector<std::function<Bytecode()>> composite_generators_;

    /**
     * @brief Generates bytecode to set a random, non-black color.
     */
    Bytecode generateNonBlackColorBytecode();

    // --- Random Value Helpers ---
    uint8_t getRandomByte();
    uint8_t getRandomNonZeroByte();
    uint8_t getRandomCoord(bool is_x);
};

} // namespace evosim