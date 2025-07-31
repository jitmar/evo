#pragma once

#include <cstdint>
#include <unordered_map>
#include <utility> // For std::pair

#include "nlohmann/json.hpp" // For JSON serialization

namespace evosim {

/**
 * @brief Defines the instruction set for the BytecodeVM.
 * @warning This enum is the source of truth for opcodes.
 */
enum class Opcode : uint8_t {
    NOP = 0x00,         ///< No operation
    PUSH = 0x01,        ///< Push value to stack
    POP = 0x02,         ///< Pop value from stack
    ADD = 0x03,         ///< Add top two stack values
    SUB = 0x04,         ///< Subtract top two stack values
    MUL = 0x05,         ///< Multiply top two stack values
    DIV = 0x06,         ///< Divide top two stack values
    MOD = 0x07,         ///< Modulo of top two stack values
    AND = 0x08,         ///< Bitwise AND of top two stack values
    OR = 0x09,          ///< Bitwise OR of top two stack values
    XOR = 0x0A,         ///< Bitwise XOR of top two stack values
    NOT = 0x0B,         ///< Bitwise NOT of top value
    JMP = 0x0C,         ///< Unconditional jump
    JZ = 0x0D,          ///< Jump if zero
    JNZ = 0x0E,         ///< Jump if not zero
    CALL = 0x0F,        ///< Call subroutine
    RET = 0x10,         ///< Return from subroutine
    LOAD = 0x11,        ///< Load from memory
    STORE = 0x12,       ///< Store to memory
    DRAW_PIXEL = 0x13,  ///< Draw pixel at current position
    SET_X = 0x14,       ///< Set X coordinate from stack
    SET_Y = 0x15,       ///< Set Y coordinate from stack
    SET_COLOR_R = 0x16, ///< Set red color channel from stack
    SET_COLOR_G = 0x17, ///< Set green color channel from stack
    SET_COLOR_B = 0x18, ///< Set blue color channel from stack
    RANDOM = 0x19,      ///< Push random value to stack
    DUP = 0x1A,         ///< Duplicate top stack value
    SWAP = 0x1B,        ///< Swap top two stack values
    ROT = 0x1C,         ///< Rotate top three stack values
    DRAW_CIRCLE = 0x1D, ///< Draw circle at current position (radius from stack)
    // New drawing primitives
    DRAW_RECTANGLE = 0x1E,    ///< Draw rectangle at current position (width, height from stack)
    DRAW_LINE = 0x1F,         ///< Draw line from current position to (x2, y2) from stack
    DRAW_BEZIER_CURVE = 0x20, ///< Draw Bezier curve (cx, cy, ex, ey from stack)
    DRAW_TRIANGLE = 0x21,     ///< Draw triangle with vertices (x1,y1), (x2,y2), (x3,y3) from stack
    HALT = 0xFF,        ///< Halt execution
};

namespace detail {

/**
 * @brief Provides a single source of truth for opcode metadata.
 *
 * This avoids scattering opcode properties (like string representation and
 * operand size) across multiple switch statements, which improves maintainability.
 * When a new opcode is added, it only needs to be added to the enum and this map.
 *
 * @return A constant reference to a map from Opcode to its metadata.
 */
inline const std::unordered_map<Opcode, std::pair<const char*, int>>& get_opcode_metadata() {
    // Using a static local variable ensures thread-safe, one-time initialization.
    static const std::unordered_map<Opcode, std::pair<const char*, int>> metadata = {
        {Opcode::NOP, {"NOP", 0}},
        {Opcode::PUSH, {"PUSH", 1}},
        {Opcode::POP, {"POP", 0}},
        {Opcode::ADD, {"ADD", 0}},
        {Opcode::SUB, {"SUB", 0}},
        {Opcode::MUL, {"MUL", 0}},
        {Opcode::DIV, {"DIV", 0}},
        {Opcode::MOD, {"MOD", 0}},
        {Opcode::AND, {"AND", 0}},
        {Opcode::OR, {"OR", 0}},
        {Opcode::XOR, {"XOR", 0}},
        {Opcode::NOT, {"NOT", 0}},
        {Opcode::JMP, {"JMP", 1}},
        {Opcode::JZ, {"JZ", 1}},
        {Opcode::JNZ, {"JNZ", 1}},
        {Opcode::CALL, {"CALL", 1}},
        {Opcode::RET, {"RET", 0}},
        {Opcode::LOAD, {"LOAD", 1}},
        {Opcode::STORE, {"STORE", 1}},
        {Opcode::DRAW_PIXEL, {"DRAW_PIXEL", 0}},
        {Opcode::SET_X, {"SET_X", 1}},
        {Opcode::SET_Y, {"SET_Y", 1}},
        {Opcode::SET_COLOR_R, {"SET_COLOR_R", 0}},
        {Opcode::SET_COLOR_G, {"SET_COLOR_G", 0}},
        {Opcode::SET_COLOR_B, {"SET_COLOR_B", 0}},
        {Opcode::RANDOM, {"RANDOM", 0}},
        {Opcode::DUP, {"DUP", 0}},
        {Opcode::SWAP, {"SWAP", 0}},
        {Opcode::ROT, {"ROT", 0}},
        {Opcode::DRAW_CIRCLE, {"DRAW_CIRCLE", 0}},
        {Opcode::DRAW_RECTANGLE, {"DRAW_RECTANGLE", 0}},
        {Opcode::DRAW_LINE, {"DRAW_LINE", 0}},
        {Opcode::DRAW_BEZIER_CURVE, {"DRAW_BEZIER_CURVE", 0}},
        {Opcode::DRAW_TRIANGLE, {"DRAW_TRIANGLE", 0}},
        {Opcode::HALT, {"HALT", 0}}};
    return metadata;
}

} // namespace detail

/**
 * @brief Serializes an Opcode to its string representation for JSON.
 *
 * This allows for human-readable opcodes in JSON output, which is useful for
 * debugging and client communication. It's defined here to keep the Opcode
 * definitions and their serialization logic co-located.
 *
 * @param j The nlohmann::json object to write to.
 * @param op The Opcode to serialize.
 */
inline void to_json(nlohmann::json& j, const Opcode& op) {
    const auto& metadata = detail::get_opcode_metadata();
    auto it = metadata.find(op);
    if (it != metadata.end()) {
        j = it->second.first;
    } else {
        j = "UNKNOWN";
    }
}

/**
 * @brief Returns the size of an opcode's operand in bytes.
 *
 * This function provides a single source of truth for the operand size of each
 * opcode, which is crucial for bytecode parsing, execution, and mutation.
 *
 * @param op The opcode to check.
 * @return The operand size in bytes (0 or 1). Returns -1 for an unknown opcode.
 */
inline int getOperandSize(Opcode op) {
    const auto& metadata = detail::get_opcode_metadata();
    auto it = metadata.find(op);
    if (it != metadata.end()) {
        // The operand size is the second element in the pair.
        return it->second.second;
    }
    return -1; // Unknown opcode
}

} // namespace evosim