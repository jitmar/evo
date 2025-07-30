#include "core/bytecode_vm.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"
#include "core/bytecode_generator.h"

namespace evosim {

BytecodeVM::BytecodeVM(const Config& config)
    : config_(config)
    , rng_(std::random_device{}()) {
    initializeState();
}

BytecodeVM::Image BytecodeVM::execute(const Bytecode& bytecode) const {
    reset();
    
    // Copy bytecode to memory
    size_t copy_size = std::min(bytecode.size(), static_cast<size_t>(config_.memory_size));
    std::copy(bytecode.begin(), bytecode.begin() + static_cast<ptrdiff_t>(copy_size), state_.memory.begin());
    executionLoop();
    return canvas_.clone();
}

BytecodeVM::Image BytecodeVM::execute(const Bytecode& bytecode, const VMState& initial_state) const {
    state_ = initial_state;
    canvas_ = cv::Mat::zeros(static_cast<int>(config_.image_height), static_cast<int>(config_.image_width), CV_8UC3);
    resetStats();
    state_.running = true; // Ensure VM is marked as running
    
    // Copy bytecode to memory
    size_t copy_size = std::min(bytecode.size(), static_cast<size_t>(config_.memory_size));
    std::copy(bytecode.begin(), bytecode.begin() + static_cast<ptrdiff_t>(copy_size), state_.memory.begin());
    executionLoop();
    return canvas_.clone();
}

void BytecodeVM::reset() const {
    initializeState();
    resetStats();
}

bool BytecodeVM::validateBytecode(const Bytecode& bytecode) const {
    if (bytecode.empty()) {
        return false;
    }
    
    for (size_t i = 0; i < bytecode.size(); ) {
        auto opcode = static_cast<Opcode>(bytecode[i]);
        int operand_size = evosim::getOperandSize(opcode);

        if (operand_size == -1) {
            spdlog::warn("Validation failed: Unknown opcode 0x{:02x} at address {}", static_cast<int>(opcode), i);
            return false; // Unknown opcode
        }

        // Check if the instruction's operand would read past the end of the bytecode
        if (i + static_cast<size_t>(operand_size) >= bytecode.size()) {
            spdlog::warn("Validation failed: Incomplete instruction at end of bytecode. Addr: {}, Opcode: 0x{:02x}", i, static_cast<int>(opcode));
            return false; // Missing operand for the last instruction
        }
        i += (1 + static_cast<size_t>(operand_size));
    }
    
    return true;
}

std::string BytecodeVM::disassemble(const Bytecode& bytecode) const {
    std::ostringstream oss;
    oss << "Disassembly:\n";
    
    for (size_t i = 0; i < bytecode.size(); ) {
        auto opcode = static_cast<Opcode>(bytecode[i]);
        int operand_size = evosim::getOperandSize(opcode);

        // Address
        oss << std::hex << std::setw(4) << std::setfill('0') << i << ": ";

        // Raw bytes
        std::string raw_bytes;
        raw_bytes += (std::ostringstream() << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytecode[i])).str();
        if (operand_size > 0 && i + 1 < bytecode.size()) {
            raw_bytes += " " + (std::ostringstream() << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytecode[i + 1])).str();
        }
        oss << std::left << std::setw(8) << std::setfill(' ') << raw_bytes;

        // Mnemonic (reusing the existing to_json function from opcodes.h)
        nlohmann::json j;
        to_json(j, opcode);
        oss << std::left << std::setw(14) << std::setfill(' ') << j.get<std::string>();

        // Operand value
        if (operand_size > 0) {
            if (i + 1 < bytecode.size()) {
                oss << static_cast<int>(bytecode[i + 1]);
            } else {
                oss << "(missing)";
            }
        }

        oss << "\n";
        i += static_cast<size_t>(1 + std::max(0, operand_size));
    }
    
    return oss.str();
}

template <typename Operation>
bool BytecodeVM::executeBinaryOp(Operation op, const char* error_on_zero) const {
    uint8_t b, a;
    if (!popStack(b) || !popStack(a)) {
        last_stats_.error_message = "Stack underflow";
        return false;
    }
    if (error_on_zero && b == 0) {
        last_stats_.error_message = error_on_zero;
        return false;
    }
    if (!pushStack(static_cast<uint8_t>(op(a, b)))) {
        last_stats_.error_message = "Stack overflow";
        return false;
    }
    // Opcodes using this helper are 1 byte long.
    state_.pc++;
    // 2 pops, 1 push
    last_stats_.stack_operations += 3;
    return true;
}

template <typename Operation>
bool BytecodeVM::executeUnaryOp(Operation op) const {
    uint8_t a;
    if (!popStack(a)) {
        last_stats_.error_message = "Stack underflow";
        return false;
    }
    if (!pushStack(static_cast<uint8_t>(op(a)))) {
        last_stats_.error_message = "Stack overflow";
        return false;
    }

    // Opcodes using this helper are 1 byte long.
    state_.pc++;
    // 1 pop, 1 push
    last_stats_.stack_operations += 2;
    return true;
}

bool BytecodeVM::executeSetColor(uint8_t& color_channel) const {
    uint8_t value;
    if (!popStack(value)) {
        last_stats_.error_message = "Stack underflow";
        return false;
    }
    color_channel = value;
    state_.pc++;
    last_stats_.stack_operations++;
    return true;
}

void BytecodeVM::executionLoop() const {
    // Execute bytecode
    while (state_.running) {
        
        if (state_.pc >= state_.memory.size()) {
            spdlog::info("Program counter reached memory size");
            break;
        }

        if (last_stats_.instructions_executed >= config_.max_instructions) {
            spdlog::info("Max instructions reached: {}", config_.max_instructions);
            break;
        }

        uint8_t opcode = state_.memory[state_.pc];
        uint8_t operand = (state_.pc + 1 < state_.memory.size()) ?
            state_.memory[state_.pc + 1] : 0;
        
        if (!executeInstruction(static_cast<Opcode>(opcode), operand)) {
            break;
        }
        
        last_stats_.instructions_executed++;
    }
    
    last_stats_.halted_normally = !state_.running;
}

bool BytecodeVM::executeInstruction(Opcode opcode, uint8_t operand) const {
    switch (opcode) {
        case Opcode::NOP:
            state_.pc++;
            return true;
            
        case Opcode::PUSH:
            if (!pushStack(operand)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc += 2; // opcode + operand
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::POP:
            uint8_t value;
            if (!popStack(value)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::ADD:
            return executeBinaryOp([](uint8_t a, uint8_t b) { return a + b; });
            
        case Opcode::SUB:
            return executeBinaryOp([](uint8_t a, uint8_t b) { return a - b; });
            
        case Opcode::MUL:
            return executeBinaryOp([](uint8_t a, uint8_t b) { return a * b; });
            
        case Opcode::DIV:
            return executeBinaryOp([](uint8_t a, uint8_t b) { return a / b; }, "Division by zero");
            
        case Opcode::MOD:
            return executeBinaryOp([](uint8_t a, uint8_t b) { return a % b; }, "Modulo by zero");
            
        case Opcode::AND:
            return executeBinaryOp([](uint8_t a, uint8_t b) { return a & b; });
            
        case Opcode::OR:
            return executeBinaryOp([](uint8_t a, uint8_t b) { return a | b; });
            
        case Opcode::XOR:
            return executeBinaryOp([](uint8_t a, uint8_t b) { return a ^ b; });
            
        case Opcode::NOT:
            return executeUnaryOp([](uint8_t a) { return ~a; });
            
        case Opcode::JMP:
            state_.pc = operand;
            return true;
            
        case Opcode::JZ:
            uint8_t a;
            if (!peekStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (a == 0) {
                state_.pc = operand;
            } else {
                state_.pc += 2;
            }
            return true;
            
        case Opcode::JNZ:
            if (!peekStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (a != 0) {
                state_.pc = operand;
            } else {
                state_.pc += 2;
            }
            return true;
            
        case Opcode::CALL:
            // Simple call implementation - just jump
            state_.pc = operand;
            return true;
            
        case Opcode::RET:
            // Simple return implementation - just continue
            state_.pc++;
            return true;
            
        case Opcode::LOAD:
            if (operand < state_.memory.size()) {
                if (!pushStack(state_.memory[operand])) {
                    last_stats_.error_message = "Stack overflow";
                    return false;
                }
            } else {
                last_stats_.error_message = "Memory access out of bounds";
                return false;
            }
            state_.pc += 2;
            last_stats_.memory_operations++;
            last_stats_.stack_operations++; // 1 push
            return true;
            
        case Opcode::STORE:
            if (!popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (operand < state_.memory.size()) {
                state_.memory[operand] = a;
            } else {
                last_stats_.error_message = "Memory access out of bounds";
                return false;
            }
            state_.pc += 2;
            last_stats_.memory_operations++;
            last_stats_.stack_operations++; // 1 pop
            return true;
            
        case Opcode::DRAW_PIXEL:
            drawPixel();
            state_.pc++;
            last_stats_.pixels_drawn++;
            return true;
            
        case Opcode::SET_X:
            state_.x = operand;
            state_.pc += 2;
            return true;
            
        case Opcode::SET_Y:
            state_.y = operand;
            state_.pc += 2;
            return true;
            
        case Opcode::SET_COLOR_R: {
            return executeSetColor(state_.color_r);
        }

        case Opcode::SET_COLOR_G: {
            return executeSetColor(state_.color_g);
        }

        case Opcode::SET_COLOR_B: {
            return executeSetColor(state_.color_b);
        }
        case Opcode::RANDOM:
            if (!pushStack(static_cast<uint8_t>(rng_() % 256))) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::DUP:
            if (!peekStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(a)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::SWAP:
            uint8_t b;
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(b) || !pushStack(a)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            // 2 pops, 2 pushes
            last_stats_.stack_operations += 4;
            return true;
            
        case Opcode::ROT:
            uint8_t c;
            if (!popStack(c) || !popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(b) || !pushStack(c) || !pushStack(a)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            // 3 pops, 3 pushes
            last_stats_.stack_operations += 6;
            return true;

        case Opcode::DRAW_CIRCLE:
            uint8_t radius;
            if (!popStack(radius)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            drawCircle(radius);
            state_.pc++;
            // Count as one drawing operation, consistent with DRAW_PIXEL
            last_stats_.pixels_drawn++;
            // 1 pop
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::DRAW_RECTANGLE:
            uint8_t height, width;
            if (!popStack(height) || !popStack(width)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            drawRectangle(width, height);
            state_.pc++;
            // Consistent with other draw ops
            last_stats_.pixels_drawn++;
            // 2 pops
            last_stats_.stack_operations += 2;
            return true;

        case Opcode::DRAW_LINE: {
            uint8_t y2, x2;
            if (!popStack(y2) || !popStack(x2)) {
                last_stats_.error_message = "Stack underflow for DRAW_LINE";
                return false;
            }
            drawLine(static_cast<int>(state_.x), static_cast<int>(state_.y), x2, y2);
            state_.pc++;
            last_stats_.pixels_drawn++;
            last_stats_.stack_operations += 2;
            return true;
        }

        case Opcode::DRAW_BEZIER_CURVE: {
            uint8_t ey, ex, cy, cx; // End point (ex, ey), Control point (cx, cy)
            if (!popStack(ey) || !popStack(ex) || !popStack(cy) || !popStack(cx)) {
                last_stats_.error_message = "Stack underflow for DRAW_BEZIER_CURVE";
                return false;
            }
            // The curve starts at the current VM position (state_.x, state_.y)
            drawQuadraticBezier(static_cast<int>(state_.x), static_cast<int>(state_.y), cx, cy, ex, ey);
            state_.pc++;
            last_stats_.pixels_drawn++;
            last_stats_.stack_operations += 4;
            return true;
        }

        case Opcode::DRAW_TRIANGLE: {
            uint8_t y3, x3, y2, x2, y1, x1;
            if (!popStack(y3) || !popStack(x3) || !popStack(y2) || !popStack(x2) || !popStack(y1) || !popStack(x1)) {
                last_stats_.error_message = "Stack underflow for DRAW_TRIANGLE";
                return false;
            }

            drawLine(x1, y1, x2, y2);
            drawLine(x2, y2, x3, y3);
            drawLine(x3, y3, x1, y1);

            state_.pc++;
            last_stats_.pixels_drawn++;
            last_stats_.stack_operations += 6;
            return true;
        }

        case Opcode::HALT:
            state_.running = false;
            return false;
            
        default:
            last_stats_.error_message = "Unknown opcode: " + std::to_string(static_cast<int>(opcode));
            return false;
    }
}

bool BytecodeVM::pushStack(uint8_t value) const {
    if (state_.stack.size() >= config_.stack_size) {
        return false;
    }
    state_.stack.push_back(value);
    return true;
}

bool BytecodeVM::popStack(uint8_t& value) const {
    if (state_.stack.empty()) {
        return false;
    }
    value = state_.stack.back();
    state_.stack.pop_back();
    return true;
}

bool BytecodeVM::peekStack(uint8_t& value) const {
    if (state_.stack.empty()) {
        return false;
    }
    value = state_.stack.back();
    return true;
}

void BytecodeVM::drawPixel() const {
    if (isInBounds(state_.x, state_.y)) {
        cv::Vec3b& pixel = canvas_.at<cv::Vec3b>(static_cast<int>(state_.y), static_cast<int>(state_.x));
        pixel[0] = state_.color_b; // Blue
        pixel[1] = state_.color_g; // Green
        pixel[2] = state_.color_r; // Red
    }
}

void BytecodeVM::drawCircle(uint8_t radius) const {
    // Check if x and y are within the valid range for int before casting
    if (state_.x > INT_MAX || state_.y > INT_MAX) {
        // Log an error or handle the out-of-range values appropriately
        last_stats_.error_message = "X or Y coordinate out of range for int conversion";
        return;
    }

    cv::Point center(static_cast<int>(state_.x), static_cast<int>(state_.y));
    if (isInBounds(state_.x, state_.y)) {
          cv::circle(canvas_, center, radius, cv::Scalar(state_.color_b, state_.color_g, state_.color_r), cv::FILLED, cv::LINE_8);
    }
}

void BytecodeVM::drawRectangle(uint8_t width, uint8_t height) const {
    if (state_.x > INT_MAX || state_.y > INT_MAX) {
        last_stats_.error_message = "X or Y coordinate out of range for int conversion";
        return;
    }

    // Draw the four sides of the rectangle from the current (x, y)
    int x_start = static_cast<int>(state_.x);
    int y_start = static_cast<int>(state_.y);
    int x_end = x_start + width;
    int y_end = y_start + height;

    drawLine(x_start, y_start, x_end, y_start); // Top
    drawLine(x_start, y_end, x_end, y_end);     // Bottom
    drawLine(x_start, y_start, x_start, y_end); // Left
    drawLine(x_end, y_start, x_end, y_end);     // Right
}

void BytecodeVM::drawLine(int x1, int y1, int x2, int y2) const {
    // Bresenham's line algorithm for integer-only arithmetic.
    const int dx = std::abs(x2 - x1);
    const int dy = -std::abs(y2 - y1);
    const int sx = (x1 < x2) ? 1 : -1;
    const int sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        // Ensure coordinates are non-negative before casting to unsigned for bounds check.
        if (x1 >= 0 && y1 >= 0 && isInBounds(static_cast<uint32_t>(x1), static_cast<uint32_t>(y1))) {
            cv::Vec3b& pixel = canvas_.at<cv::Vec3b>(y1, x1);
            pixel[0] = state_.color_b;
            pixel[1] = state_.color_g;
            pixel[2] = state_.color_r;
        }
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void BytecodeVM::drawQuadraticBezier(int x0, int y0, int x1, int y1, int x2, int y2) const {
    // Draw the curve by interpolating points and connecting them with short lines
    // to ensure a continuous, gap-free curve.
    const int steps = 30; // More steps result in a smoother curve.
    int prev_x = x0;
    int prev_y = y0;

    for (int i = 1; i <= steps; ++i) {
        const float t = static_cast<float>(i) / steps;
        const float u = 1.0f - t;

        // Quadratic Bezier formula: B(t) = (1-t)^2*P0 + 2(1-t)t*P1 + t^2*P2
        const int x = static_cast<int>(std::round(
            u * u * static_cast<float>(x0) +
            2.0f * u * t * static_cast<float>(x1) +
            t * t * static_cast<float>(x2)));
        const int y = static_cast<int>(std::round(
            u * u * static_cast<float>(y0) +
            2.0f * u * t * static_cast<float>(y1) +
            t * t * static_cast<float>(y2)));

        // Draw a line segment from the previous point to the current one.
        // This avoids gaps that can appear when just drawing individual pixels.
        drawLine(prev_x, prev_y, x, y);

        prev_x = x;
        prev_y = y;
    }
}

bool BytecodeVM::isInBounds(uint32_t x, uint32_t y) const {
    return x < config_.image_width && y < config_.image_height;
}

void BytecodeVM::initializeState() const {
    state_.stack.clear();
    state_.memory.resize(config_.memory_size, 0);
    state_.pc = 0;
    state_.x = 0;
    state_.y = 0;
    state_.color_r = 0;
    state_.color_g = 0;
    state_.color_b = 0;
    state_.running = true;
    
    canvas_ = cv::Mat::zeros(static_cast<int>(config_.image_height), static_cast<int>(config_.image_width), CV_8UC3);
    resetStats();
}

void BytecodeVM::resetStats() const {
    last_stats_.instructions_executed = 0;
    last_stats_.pixels_drawn = 0;
    last_stats_.stack_operations = 0;
    last_stats_.memory_operations = 0;
    last_stats_.halted_normally = false;
    last_stats_.error_message.clear();
}

BytecodeVM::Config BytecodeVM::getConfig() const {
    return config_;
}

BytecodeVM::ExecutionStats BytecodeVM::getLastStats() const {
    return last_stats_;
}

BytecodeVM::VMState BytecodeVM::getLastState() const {
    return state_;
}

BytecodeVM::Bytecode BytecodeVM::generateRandomBytecode(uint32_t size) const {
    if (size == 0) {
        return {};
    }

    // Delegate the complex generation to the dedicated BytecodeGenerator.
    // This centralizes the logic for creating meaningful, drawable bytecode.
    BytecodeGenerator generator(config_.image_width, config_.image_height);

    // Determine a reasonable number of primitives based on the requested size.
    // A circle primitive is ~12 bytes. We aim for about half the space to be
    // structured primitives, leaving room for mutations.
    size_t num_primitives = std::max(1u, size / 25);
    Bytecode bytecode = generator.generateInitialBytecode(num_primitives);

    // --- Adjust size and terminate ---
    if (bytecode.size() > size) {
        // Truncate if too long.
        bytecode.resize(size);
    } else {
        // Pad with NOPs if too short.
        bytecode.resize(size, static_cast<uint8_t>(Opcode::NOP));
    }

    // Ensure the program always terminates.
    bytecode.back() = static_cast<uint8_t>(Opcode::HALT);

    return bytecode;
}

} // namespace evosim 