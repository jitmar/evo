#pragma once

#include <vector>
#include <cstdint>
#include <random>
#include <functional>
#include "nlohmann/json.hpp"
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

    /**
     * @brief Configuration for generating initial organism bytecode.
     */
    struct Config {
        uint32_t min_units_per_organism = 1;    ///< Min HALT-terminated units in a new genome.
        uint32_t max_units_per_organism = 5;    ///< Max HALT-terminated units in a new genome.
        double primitive_unit_probability = 0.4; ///< Chance a unit is a known-good primitive.
        uint32_t min_opcodes_per_composite = 5;  ///< Min opcodes in a random composite unit.
        uint32_t max_opcodes_per_composite = 20; ///< Max opcodes in a random composite unit.
    };

    /**
     * @brief Deprecated constructor. Use the constructor that accepts a Config object.
     */
    BytecodeGenerator(uint32_t image_width, uint32_t image_height, double composite_chance = 0.25);

    /**
     * @brief Preferred constructor that uses a detailed configuration.
     */
    BytecodeGenerator(const Config& config, uint32_t image_width, uint32_t image_height);

    /**
     * @brief Generates a complete, multi-unit bytecode sequence for a single organism.
     * @return A vector of bytes representing the organism's full genome.
     */
    Bytecode generateOrganismBytecode() const;

    Bytecode generateInitialBytecode(size_t num_primitives) const;

    // --- Random Primitive Generators (for initial population) ---
    Bytecode createNonBlackCirclePrimitive() const;
    Bytecode createLinePrimitive() const;
    Bytecode createBezierCurvePrimitive() const;
    Bytecode createTrianglePrimitive() const;
    Bytecode createNonBlackRectanglePrimitive() const;

    // --- Composite Generators (combining primitives) ---
    Bytecode createStickFigureComposite();

    // --- Parameterized Primitive Builders (for composites) ---
    Bytecode createCircle(uint8_t x, uint8_t y, uint8_t radius) const;
    Bytecode createRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h) const;
    Bytecode createLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) const;
    Bytecode createTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3) const;
    Bytecode createBezierCurve(uint8_t x0, uint8_t y0, uint8_t cx, uint8_t cy, uint8_t ex, uint8_t ey) const;

private:
    Config config_;
    uint32_t image_width_;
    uint32_t image_height_;
    double composite_chance_;
    mutable std::mt19937 rng_;
    std::vector<std::function<Bytecode()>> primitive_generators_;
    std::vector<std::function<Bytecode()>> composite_generators_;

    /**
     * @brief Generates bytecode to set a random, non-black color.
     */
    Bytecode generateNonBlackColorBytecode() const;

    // --- Random Value Helpers ---
    uint8_t getRandomByte() const;
    uint8_t getRandomNonZeroByte() const;
    uint8_t getRandomCoord(bool is_x) const;
};

inline void to_json(nlohmann::json& j, const BytecodeGenerator::Config& c) {
    j = nlohmann::json{
        {"min_units_per_organism", c.min_units_per_organism},
        {"max_units_per_organism", c.max_units_per_organism},
        {"primitive_unit_probability", c.primitive_unit_probability},
        {"min_opcodes_per_composite", c.min_opcodes_per_composite},
        {"max_opcodes_per_composite", c.max_opcodes_per_composite},
    };
}

inline void from_json(const nlohmann::json& j, BytecodeGenerator::Config& c) {
    // Use .value() for safety to avoid breaking on older configs.
    c.min_units_per_organism = j.value("min_units_per_organism", c.min_units_per_organism);
    c.max_units_per_organism = j.value("max_units_per_organism", c.max_units_per_organism);
    c.primitive_unit_probability = j.value("primitive_unit_probability", c.primitive_unit_probability);
    c.min_opcodes_per_composite = j.value("min_opcodes_per_composite", c.min_opcodes_per_composite);
    c.max_opcodes_per_composite = j.value("max_opcodes_per_composite", c.max_opcodes_per_composite);
}

} // namespace evosim