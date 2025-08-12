#include "core/bytecode_generator.h"
#include <algorithm>
#include <vector>

namespace evosim {

// Deprecated constructor
BytecodeGenerator::BytecodeGenerator(uint32_t image_width, uint32_t image_height, double composite_chance)
    : image_width_(image_width), image_height_(image_height), composite_chance_(composite_chance), rng_(std::random_device{}()) {
    primitive_generators_.push_back([this]() { return createNonBlackCirclePrimitive(); });
    primitive_generators_.push_back([this]() { return createLinePrimitive(); });
    primitive_generators_.push_back([this]() { return createTrianglePrimitive(); });
    primitive_generators_.push_back([this]() { return createNonBlackRectanglePrimitive(); });
    primitive_generators_.push_back([this]() { return createBezierCurvePrimitive(); });
}

// Preferred constructor
BytecodeGenerator::BytecodeGenerator(const Config& config, uint32_t image_width, uint32_t image_height)
    : config_(config), image_width_(image_width), image_height_(image_height), composite_chance_(0.0), rng_(std::random_device{}()) {
    primitive_generators_.push_back([this]() { return createNonBlackCirclePrimitive(); });
    primitive_generators_.push_back([this]() { return createLinePrimitive(); });
    primitive_generators_.push_back([this]() { return createTrianglePrimitive(); });
    primitive_generators_.push_back([this]() { return createNonBlackRectanglePrimitive(); });
    primitive_generators_.push_back([this]() { return createBezierCurvePrimitive(); });
}

BytecodeGenerator::Bytecode BytecodeGenerator::generateOrganismBytecode() const {
    Bytecode final_bytecode;
    std::uniform_int_distribution<uint32_t> num_units_dist(
        config_.min_units_per_organism,
        config_.max_units_per_organism
    );
    std::uniform_real_distribution<> probability_dist(0.0, 1.0);
    std::uniform_int_distribution<uint32_t> composite_size_dist(
        config_.min_opcodes_per_composite,
        config_.max_opcodes_per_composite
    );
    std::uniform_int_distribution<size_t> primitive_dist(0, primitive_generators_.size() - 1);

    uint32_t num_units = num_units_dist(rng_);

    for (uint32_t i = 0; i < num_units; ++i) {
        Bytecode unit;
        if (!primitive_generators_.empty() && (probability_dist(rng_) < config_.primitive_unit_probability)) {
            // Generate a primitive unit
            unit = primitive_generators_[primitive_dist(rng_)]();
        } else {
            // Generate a composite unit of "safe" random opcodes to prevent infinite loops
            const std::vector<Opcode> safe_opcodes = {
                Opcode::PUSH, Opcode::POP, Opcode::ADD, Opcode::SUB, Opcode::MUL, Opcode::DIV,
                Opcode::MOD, Opcode::AND, Opcode::OR, Opcode::XOR, Opcode::NOT, Opcode::LOAD,
                Opcode::STORE, Opcode::DRAW_PIXEL, Opcode::SET_X, Opcode::SET_Y,
                Opcode::SET_COLOR_R, Opcode::SET_COLOR_G, Opcode::SET_COLOR_B, Opcode::RANDOM,
                Opcode::DUP, Opcode::SWAP, Opcode::ROT, Opcode::DRAW_CIRCLE, Opcode::DRAW_RECTANGLE,
                Opcode::DRAW_LINE, Opcode::DRAW_BEZIER_CURVE, Opcode::DRAW_TRIANGLE
            };
            std::uniform_int_distribution<size_t> opcode_dist(0, safe_opcodes.size() - 1);

            uint32_t composite_size = composite_size_dist(rng_);
            for (uint32_t j = 0; j < composite_size; ++j) {
                Opcode opcode = safe_opcodes[opcode_dist(rng_)];
                unit.push_back(static_cast<uint8_t>(opcode));

                int operand_size = getOperandSize(opcode);
                if (operand_size > 0) {
                    // For now, just a random byte. Could be smarter (e.g., valid coords/addrs).
                    unit.push_back(getRandomByte());
                    // Decrement j because we used an extra byte for the operand.
                    if (j < composite_size -1) j++;
                }
            }
        }

        // Append the generated unit and a HALT opcode to mark its end.
        final_bytecode.insert(final_bytecode.end(), unit.begin(), unit.end());
        final_bytecode.push_back(static_cast<uint8_t>(Opcode::HALT));
    }

    return final_bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::generateInitialBytecode(size_t num_primitives) const {
    Bytecode bytecode;
    if (primitive_generators_.empty()) return bytecode;

    std::uniform_int_distribution<size_t> dist(0, primitive_generators_.size() - 1);

    for (size_t i = 0; i < num_primitives; ++i) {
        auto primitive = primitive_generators_[dist(rng_)]();
        bytecode.insert(bytecode.end(), primitive.begin(), primitive.end());
    }
    bytecode.push_back(static_cast<uint8_t>(Opcode::HALT));
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::generateNonBlackColorBytecode() const {
    Bytecode bytecode = {
        static_cast<uint8_t>(Opcode::PUSH), getRandomNonZeroByte(),
        static_cast<uint8_t>(Opcode::SET_COLOR_R),
        static_cast<uint8_t>(Opcode::PUSH), getRandomNonZeroByte(),
        static_cast<uint8_t>(Opcode::SET_COLOR_G),
         static_cast<uint8_t>(Opcode::PUSH), getRandomNonZeroByte(),
        static_cast<uint8_t>(Opcode::SET_COLOR_B)
    };
    return bytecode;
}

// --- Random Value Helpers ---

uint8_t BytecodeGenerator::getRandomByte() const {
    std::uniform_int_distribution<int> dist(0, 255);
    return static_cast<uint8_t>(dist(rng_));
}

uint8_t BytecodeGenerator::getRandomNonZeroByte() const {
    std::uniform_int_distribution<int> dist(1, 255);
    return static_cast<uint8_t>(dist(rng_));
}

uint8_t BytecodeGenerator::getRandomCoord(bool is_x) const {
    // The distribution is inclusive, so we use upper_bound - 1.
    const uint32_t upper_bound = is_x ? image_width_ : image_height_;
    if (upper_bound == 0) {
        return 0;
    }
    std::uniform_int_distribution<uint32_t> dist(0, upper_bound - 1);
    return static_cast<uint8_t>(dist(rng_));
}

// --- Random Primitive Generators ---

BytecodeGenerator::Bytecode BytecodeGenerator::createNonBlackCirclePrimitive() const {
    Bytecode bytecode = generateNonBlackColorBytecode();
    Bytecode primitive = {
        static_cast<uint8_t>(Opcode::SET_X), getRandomCoord(true),
        static_cast<uint8_t>(Opcode::SET_Y), getRandomCoord(false),
        static_cast<uint8_t>(Opcode::PUSH), getRandomByte(), // radius
        static_cast<uint8_t>(Opcode::DRAW_CIRCLE)
    };
    bytecode.insert(bytecode.end(), primitive.begin(), primitive.end());
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::createLinePrimitive() const {
    Bytecode bytecode = generateNonBlackColorBytecode();
    Bytecode primitive = {
        static_cast<uint8_t>(Opcode::SET_X), getRandomCoord(true),  // x1
        static_cast<uint8_t>(Opcode::SET_Y), getRandomCoord(false), // y1
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(true),  // x2
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(false), // y2
        static_cast<uint8_t>(Opcode::DRAW_LINE)
    };
    bytecode.insert(bytecode.end(), primitive.begin(), primitive.end());
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::createBezierCurvePrimitive() const {
    Bytecode bytecode = generateNonBlackColorBytecode();
    Bytecode primitive = {
        static_cast<uint8_t>(Opcode::SET_X), getRandomCoord(true),
        static_cast<uint8_t>(Opcode::SET_Y), getRandomCoord(false),
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(true),  // control x
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(false), // control y
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(true),  // end x
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(false), // end y
        static_cast<uint8_t>(Opcode::DRAW_BEZIER_CURVE)
    };
    bytecode.insert(bytecode.end(), primitive.begin(), primitive.end());
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::createTrianglePrimitive() const {
    Bytecode bytecode = generateNonBlackColorBytecode();
    Bytecode primitive = {
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(true),  // x1
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(false), // y1
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(true),  // x2
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(false), // y2
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(true),  // x3
        static_cast<uint8_t>(Opcode::PUSH), getRandomCoord(false), // y3
        static_cast<uint8_t>(Opcode::DRAW_TRIANGLE)
    };
    bytecode.insert(bytecode.end(), primitive.begin(), primitive.end());
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::createNonBlackRectanglePrimitive() const {
    Bytecode bytecode = generateNonBlackColorBytecode();
    Bytecode primitive = {
        static_cast<uint8_t>(Opcode::SET_X), getRandomCoord(true),
        static_cast<uint8_t>(Opcode::SET_Y), getRandomCoord(false),
        static_cast<uint8_t>(Opcode::PUSH), getRandomByte(), // width
        static_cast<uint8_t>(Opcode::PUSH), getRandomByte(), // height
        static_cast<uint8_t>(Opcode::DRAW_RECTANGLE)
    };
    bytecode.insert(bytecode.end(), primitive.begin(), primitive.end());
    return bytecode;
}

// --- Parameterized Primitive Builders (for composites) ---

BytecodeGenerator::Bytecode BytecodeGenerator::createCircle(uint8_t x, uint8_t y, uint8_t radius) const {
    return {
        static_cast<uint8_t>(Opcode::SET_X), x,
        static_cast<uint8_t>(Opcode::SET_Y), y,
        static_cast<uint8_t>(Opcode::PUSH), radius,
        static_cast<uint8_t>(Opcode::DRAW_CIRCLE)
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::createRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h) const {
    return {
        static_cast<uint8_t>(Opcode::SET_X), x,
        static_cast<uint8_t>(Opcode::SET_Y), y,
        static_cast<uint8_t>(Opcode::PUSH), w,
        static_cast<uint8_t>(Opcode::PUSH), h,
        static_cast<uint8_t>(Opcode::DRAW_RECTANGLE)
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::createLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) const {
    // The DRAW_LINE opcode uses the current (x,y) as the start point.
    return {
        static_cast<uint8_t>(Opcode::SET_X), x1,
        static_cast<uint8_t>(Opcode::SET_Y), y1,
        static_cast<uint8_t>(Opcode::PUSH), x2,
        static_cast<uint8_t>(Opcode::PUSH), y2,
        static_cast<uint8_t>(Opcode::DRAW_LINE)
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::createTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3) const {
    // The DRAW_TRIANGLE opcode takes all 6 coordinates from the stack.
    return {
        static_cast<uint8_t>(Opcode::PUSH), x1,
        static_cast<uint8_t>(Opcode::PUSH), y1,
        static_cast<uint8_t>(Opcode::PUSH), x2,
        static_cast<uint8_t>(Opcode::PUSH), y2,
        static_cast<uint8_t>(Opcode::PUSH), x3,
        static_cast<uint8_t>(Opcode::PUSH), y3,
        static_cast<uint8_t>(Opcode::DRAW_TRIANGLE)
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::createBezierCurve(uint8_t x0, uint8_t y0, uint8_t cx, uint8_t cy, uint8_t ex, uint8_t ey) const {
    // The DRAW_BEZIER_CURVE opcode uses the current (x,y) as the start point.
    return {
        static_cast<uint8_t>(Opcode::SET_X), x0,
        static_cast<uint8_t>(Opcode::SET_Y), y0,
        static_cast<uint8_t>(Opcode::PUSH), cx, static_cast<uint8_t>(Opcode::PUSH), cy, // Control point
        static_cast<uint8_t>(Opcode::PUSH), ex, static_cast<uint8_t>(Opcode::PUSH), ey, // End point
        static_cast<uint8_t>(Opcode::DRAW_BEZIER_CURVE)
    };
}

} // namespace evosim