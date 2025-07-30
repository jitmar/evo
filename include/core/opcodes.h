#pragma once

#include <cstdint>
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
    switch (op) {
        case Opcode::NOP: j = "NOP"; break;
        case Opcode::PUSH: j = "PUSH"; break;
        case Opcode::POP: j = "POP"; break;
        case Opcode::ADD: j = "ADD"; break;
        case Opcode::SUB: j = "SUB"; break;
        case Opcode::MUL: j = "MUL"; break;
        case Opcode::DIV: j = "DIV"; break;
        case Opcode::MOD: j = "MOD"; break;
        case Opcode::AND: j = "AND"; break;
        case Opcode::OR: j = "OR"; break;
        case Opcode::XOR: j = "XOR"; break;
        case Opcode::NOT: j = "NOT"; break;
        case Opcode::JMP: j = "JMP"; break;
        case Opcode::JZ: j = "JZ"; break;
        case Opcode::JNZ: j = "JNZ"; break;
        case Opcode::CALL: j = "CALL"; break;
        case Opcode::RET: j = "RET"; break;
        case Opcode::LOAD: j = "LOAD"; break;
        case Opcode::STORE: j = "STORE"; break;
        case Opcode::DRAW_PIXEL: j = "DRAW_PIXEL"; break;
        case Opcode::SET_X: j = "SET_X"; break;
        case Opcode::SET_Y: j = "SET_Y"; break;
        case Opcode::SET_COLOR_R: j = "SET_COLOR_R"; break;
        case Opcode::SET_COLOR_G: j = "SET_COLOR_G"; break;
        case Opcode::SET_COLOR_B: j = "SET_COLOR_B"; break;
        case Opcode::RANDOM: j = "RANDOM"; break;
        case Opcode::DUP: j = "DUP"; break;
        case Opcode::SWAP: j = "SWAP"; break;
        case Opcode::ROT: j = "ROT"; break;
        case Opcode::DRAW_CIRCLE: j = "DRAW_CIRCLE"; break;
        case Opcode::DRAW_RECTANGLE: j = "DRAW_RECTANGLE"; break;
        case Opcode::DRAW_LINE: j = "DRAW_LINE"; break;
        case Opcode::DRAW_BEZIER_CURVE: j = "DRAW_BEZIER_CURVE"; break;
        case Opcode::DRAW_TRIANGLE: j = "DRAW_TRIANGLE"; break;
        case Opcode::HALT: j = "HALT"; break;
        default: j = "UNKNOWN"; break;
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
    switch (op) {
        // Opcodes with a 1-byte operand
        case Opcode::PUSH: case Opcode::JMP: case Opcode::JZ: case Opcode::JNZ:
        case Opcode::CALL: case Opcode::LOAD: case Opcode::STORE:
        case Opcode::SET_X: case Opcode::SET_Y:
            return 1;

        // Opcodes with no operand
        case Opcode::NOP: case Opcode::POP: case Opcode::ADD: case Opcode::SUB:
        case Opcode::MUL: case Opcode::DIV: case Opcode::MOD: case Opcode::AND:
        case Opcode::OR: case Opcode::XOR: case Opcode::NOT: case Opcode::RET:
        case Opcode::DRAW_PIXEL: case Opcode::SET_COLOR_R: case Opcode::SET_COLOR_G:
        case Opcode::SET_COLOR_B: case Opcode::RANDOM: case Opcode::DUP: case Opcode::SWAP:
        case Opcode::ROT: case Opcode::DRAW_CIRCLE: case Opcode::DRAW_RECTANGLE:
        case Opcode::DRAW_LINE: case Opcode::DRAW_BEZIER_CURVE: case Opcode::DRAW_TRIANGLE:
        case Opcode::HALT:
            return 0;

        default: return -1; // Unknown opcode
    }
}

} // namespace evosim