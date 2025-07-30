#include "core/bytecode_generator.h"
#include "core/opcodes.h"
#include <algorithm>
#include <stdexcept>

namespace evosim {

BytecodeGenerator::BytecodeGenerator(uint32_t image_width, uint32_t image_height, double composite_chance)
    : image_width_(image_width), image_height_(image_height), composite_chance_(composite_chance) {
    if (image_width == 0 || image_height == 0) {
        throw std::invalid_argument("Image dimensions cannot be zero.");
    }
    // Seed the random number generator with a high-quality seed
    std::random_device rd;
    rng_.seed(rd());

    // Populate the library of primitive generators using lambdas
    primitive_generators_ = {
        [this]() { return createNonBlackCirclePrimitive(); },
        [this]() { return createNonBlackRectanglePrimitive(); },
        [this]() { return createLinePrimitive(); },
        [this]() { return createBezierCurvePrimitive(); },
        [this]() { return createTrianglePrimitive(); }
    };

    // Populate the library of composite generators
    composite_generators_ = {
        [this]() { return createStickFigureComposite(); }
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::generateInitialBytecode(size_t num_primitives) {
    if (primitive_generators_.empty()) {
        return {}; // Return empty bytecode if no primitives are defined
    }

    Bytecode final_bytecode;
    std::uniform_int_distribution<size_t> primitive_dist(0, primitive_generators_.size() - 1);
    std::uniform_int_distribution<size_t> composite_dist(0, composite_generators_.empty() ? 0 : composite_generators_.size() - 1);
    std::uniform_real_distribution<> chance_dist(0.0, 1.0);

    for (size_t i = 0; i < num_primitives; ++i) {
        Bytecode part_bytecode;
        // Decide whether to generate a complex composite or a simple primitive
        if (!composite_generators_.empty() && chance_dist(rng_) < composite_chance_) {
            // Select a random composite generator
            part_bytecode = composite_generators_[composite_dist(rng_)]();
        } else {
            // Select a random primitive generator
            size_t primitive_index = primitive_dist(rng_);
            part_bytecode = primitive_generators_[primitive_index]();
        }

        // Append its bytecode to the final result
        final_bytecode.insert(final_bytecode.end(), part_bytecode.begin(), part_bytecode.end());
    }

    // A complete program must end with a HALT instruction to ensure clean termination.
    final_bytecode.push_back(static_cast<uint8_t>(Opcode::HALT));
    return final_bytecode;
}

uint8_t BytecodeGenerator::getRandomByte() {
    std::uniform_int_distribution<int> dist(0, 255);
    return static_cast<uint8_t>(dist(rng_));
}

uint8_t BytecodeGenerator::getRandomNonZeroByte() {
    std::uniform_int_distribution<int> dist(1, 255);
    return static_cast<uint8_t>(dist(rng_));
}

uint8_t BytecodeGenerator::getRandomCoord(bool is_width) {
    if (is_width) {
        std::uniform_int_distribution<uint32_t> dist(0, image_width_ - 1);
        return static_cast<uint8_t>(dist(rng_));
    }
    std::uniform_int_distribution<uint32_t> dist(0, image_height_ - 1);
    return static_cast<uint8_t>(dist(rng_));
}

// --- Primitive Implementations ---

BytecodeGenerator::Bytecode BytecodeGenerator::createNonBlackCirclePrimitive() {
    Bytecode bytecode = generateNonBlackColorBytecode();
    uint8_t x = getRandomCoord(true);
    uint8_t y = getRandomCoord(false);
    uint8_t radius = getRandomNonZeroByte() / 8 + 10;
    Bytecode circle_code = createCircle(x, y, radius);
    bytecode.insert(bytecode.end(), circle_code.begin(), circle_code.end());
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::createNonBlackRectanglePrimitive() {
    Bytecode bytecode = generateNonBlackColorBytecode();
    uint8_t x = getRandomCoord(true);
    uint8_t y = getRandomCoord(false);
    uint8_t w = getRandomByte() / 4 + 5;
    uint8_t h = getRandomByte() / 4 + 5;
    Bytecode rect_code = createRectangle(x, y, w, h);
    bytecode.insert(bytecode.end(), rect_code.begin(), rect_code.end());
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::createLinePrimitive() {
    Bytecode bytecode = generateNonBlackColorBytecode();
    uint8_t x1 = getRandomCoord(true);
    uint8_t y1 = getRandomCoord(false);
    uint8_t x2 = getRandomCoord(true);
    uint8_t y2 = getRandomCoord(false);
    Bytecode line_code = createLine(x1, y1, x2, y2);
    bytecode.insert(bytecode.end(), line_code.begin(), line_code.end());
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::createBezierCurvePrimitive() {
    Bytecode bytecode = generateNonBlackColorBytecode();
    uint8_t x0 = getRandomCoord(true);
    uint8_t y0 = getRandomCoord(false);
    uint8_t cx = getRandomCoord(true);
    uint8_t cy = getRandomCoord(false);
    uint8_t ex = getRandomCoord(true);
    uint8_t ey = getRandomCoord(false);
    Bytecode curve_code = createBezierCurve(x0, y0, cx, cy, ex, ey);
    bytecode.insert(bytecode.end(), curve_code.begin(), curve_code.end());
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::createTrianglePrimitive() {
    Bytecode bytecode = generateNonBlackColorBytecode();
    uint8_t x1 = getRandomCoord(true);
    uint8_t y1 = getRandomCoord(false);
    uint8_t x2 = getRandomCoord(true);
    uint8_t y2 = getRandomCoord(false);
    uint8_t x3 = getRandomCoord(true);
    uint8_t y3 = getRandomCoord(false);
    Bytecode tri_code = createTriangle(x1, y1, x2, y2, x3, y3);
    bytecode.insert(bytecode.end(), tri_code.begin(), tri_code.end());
    return bytecode;
}

BytecodeGenerator::Bytecode BytecodeGenerator::createStickFigureComposite() {
    Bytecode final_bytecode = generateNonBlackColorBytecode();

    // 1. Define proportions using int to avoid overflow during calculations
    const int head_radius = getRandomNonZeroByte() / 16 + 8; // Range: 8-23
    const int torso_length = head_radius * 2;
    const int limb_length = head_radius;

    // 2. Pick a random center for the figure
    const int center_x = getRandomCoord(true);
    const int center_y = getRandomCoord(false);

    // Helper to clamp values into the valid uint8_t range for bytecode operands
    auto clamp = [](int val) { return static_cast<uint8_t>(std::max(0, std::min(255, val))); };

    // 3. Calculate absolute coordinates for each part
    const uint8_t head_x = clamp(center_x);
    const uint8_t head_y = clamp(center_y - torso_length / 2); // Center figure vertically

    const uint8_t torso_top_y = clamp(head_y + head_radius);
    const uint8_t torso_bottom_y = clamp(torso_top_y + torso_length);
    const uint8_t arm_y = clamp(torso_top_y + torso_length / 4);

    // 4. Generate and combine bytecode for each part using the builder functions
    Bytecode head = createCircle(head_x, head_y, clamp(head_radius));
    final_bytecode.insert(final_bytecode.end(), head.begin(), head.end());

    Bytecode torso = createLine(head_x, torso_top_y, head_x, torso_bottom_y);
    final_bytecode.insert(final_bytecode.end(), torso.begin(), torso.end());

    Bytecode left_arm = createLine(head_x, arm_y, clamp(head_x - limb_length), clamp(arm_y + limb_length / 2));
    Bytecode right_arm = createLine(head_x, arm_y, clamp(head_x + limb_length), clamp(arm_y + limb_length / 2));
    final_bytecode.insert(final_bytecode.end(), left_arm.begin(), left_arm.end());
    final_bytecode.insert(final_bytecode.end(), right_arm.begin(), right_arm.end());

    Bytecode left_leg = createLine(head_x, torso_bottom_y, clamp(head_x - limb_length), clamp(torso_bottom_y + limb_length));
    Bytecode right_leg = createLine(head_x, torso_bottom_y, clamp(head_x + limb_length), clamp(torso_bottom_y + limb_length));
    final_bytecode.insert(final_bytecode.end(), left_leg.begin(), left_leg.end());
    final_bytecode.insert(final_bytecode.end(), right_leg.begin(), right_leg.end());

    return final_bytecode;
}

// --- Parameterized Primitive Builders ---

BytecodeGenerator::Bytecode BytecodeGenerator::createCircle(uint8_t x, uint8_t y, uint8_t radius) {
    return {
        (uint8_t)Opcode::SET_X, x,
        (uint8_t)Opcode::SET_Y, y,
        (uint8_t)Opcode::PUSH, radius,
        (uint8_t)Opcode::DRAW_CIRCLE,
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::createRectangle(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    return {
        (uint8_t)Opcode::SET_X, x,
        (uint8_t)Opcode::SET_Y, y,
        (uint8_t)Opcode::PUSH, w,
        (uint8_t)Opcode::PUSH, h,
        (uint8_t)Opcode::DRAW_RECTANGLE,
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::createLine(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    return {
        (uint8_t)Opcode::SET_X, x1,
        (uint8_t)Opcode::SET_Y, y1,
        (uint8_t)Opcode::PUSH, x2,
        (uint8_t)Opcode::PUSH, y2,
        (uint8_t)Opcode::DRAW_LINE,
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::createTriangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint8_t x3, uint8_t y3) {
    return {
        (uint8_t)Opcode::PUSH, x1, (uint8_t)Opcode::PUSH, y1,
        (uint8_t)Opcode::PUSH, x2, (uint8_t)Opcode::PUSH, y2,
        (uint8_t)Opcode::PUSH, x3, (uint8_t)Opcode::PUSH, y3,
        (uint8_t)Opcode::DRAW_TRIANGLE,
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::createBezierCurve(uint8_t x0, uint8_t y0, uint8_t cx, uint8_t cy, uint8_t ex, uint8_t ey) {
    return {
        (uint8_t)Opcode::SET_X, x0, (uint8_t)Opcode::SET_Y, y0,
        (uint8_t)Opcode::PUSH, cx, (uint8_t)Opcode::PUSH, cy,
        (uint8_t)Opcode::PUSH, ex, (uint8_t)Opcode::PUSH, ey,
        (uint8_t)Opcode::DRAW_BEZIER_CURVE,
    };
}

BytecodeGenerator::Bytecode BytecodeGenerator::generateNonBlackColorBytecode() {
    uint8_t r = getRandomByte();
    uint8_t g = getRandomByte();
    uint8_t b = getRandomByte();

    // Ensure color is not black to be visible on the default background
    if (r == 0 && g == 0 && b == 0) {
        b = 128; // Just make one channel non-zero
    }

    return {(uint8_t)Opcode::PUSH, r, (uint8_t)Opcode::SET_COLOR_R,
            (uint8_t)Opcode::PUSH, g, (uint8_t)Opcode::SET_COLOR_G,
            (uint8_t)Opcode::PUSH, b, (uint8_t)Opcode::SET_COLOR_B};
}

} // namespace evosim