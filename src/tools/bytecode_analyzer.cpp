#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <map>
#include <functional>
#include <cstring> // For std::memcpy

// We need the Opcode definition from the core library.
// This path is assumed based on the project structure.
#include "core/bytecode_vm.h"

namespace evosim {

// A helper struct to hold information about each opcode.
struct OpcodeInfo {
    std::string name;
    size_t size; // Total size in bytes, including the opcode itself
    std::function<void(const std::vector<uint8_t>&, size_t)> print_args;
};

// Forward declarations for argument printing functions
void print_no_args(const std::vector<uint8_t>&, size_t);
void print_reg_float(const std::vector<uint8_t>&, size_t);
void print_reg_reg(const std::vector<uint8_t>&, size_t);
void print_reg_reg_reg(const std::vector<uint8_t>&, size_t);
void print_5_regs(const std::vector<uint8_t>&, size_t);

/**
 * @brief Maps opcodes to their metadata for disassembly.
 *
 * @warning This map is the core of the disassembler and MUST be kept
 * in sync with the Opcode enum in `include/core/bytecode_vm.h`.
 * If you add, remove, or change an opcode in the VM, you must update it here.
 */
const std::map<Opcode, OpcodeInfo> opcode_map = {
    
    {Opcode::NOP = 0x00,         {"NOP",           1, print_no_args}},///< No operation
    {Opcode::PUSH = 0x01,        {"PUSH",           1, print_push_args}},///< Push value to stack
    {Opcode::POP = 0x02,         ///< Pop value from stack
    {Opcode::ADD = 0x03,         ///< Add top two stack values
    {Opcode::SUB = 0x04,         ///< Subtract top two stack values
    {Opcode::MUL = 0x05,         ///< Multiply top two stack values
    {Opcode::DIV = 0x06,         ///< Divide top two stack values
    {Opcode::MOD = 0x07,         ///< Modulo of top two stack values
    {Opcode::AND = 0x08,         ///< Bitwise AND of top two stack values
    {Opcode::OR = 0x09,          ///< Bitwise OR of top two stack values
    {Opcode::XOR = 0x0A,         ///< Bitwise XOR of top two stack values
    {Opcode::NOT = 0x0B,         ///< Bitwise NOT of top value
    {Opcode::JMP = 0x0C,         ///< Unconditional jump
    {Opcode::JZ = 0x0D,          ///< Jump if zero
    {Opcode::JNZ = 0x0E,         ///< Jump if not zero
    {Opcode::CALL = 0x0F,        ///< Call subroutine
    {Opcode::RET = 0x10,         ///< Return from subroutine
    {Opcode::LOAD = 0x11,        ///< Load from memory
    {Opcode::STORE = 0x12,       ///< Store to memory
    {Opcode::DRAW_PIXEL = 0x13,  ///< Draw pixel at current position
    {Opcode::SET_X = 0x14,       ///< Set X coordinate from stack
    {Opcode::SET_Y = 0x15,       ///< Set Y coordinate from stack
    {Opcode::SET_COLOR_R = 0x16, ///< Set red color channel from stack
    {Opcode::SET_COLOR_G = 0x17, ///< Set green color channel from stack
    {Opcode::SET_COLOR_B = 0x18, ///< Set blue color channel from stack
    {Opcode::RANDOM = 0x19,      ///< Push random value to stack
    {Opcode::DUP = 0x1A,         ///< Duplicate top stack value
    {Opcode::SWAP = 0x1B,        ///< Swap top two stack values
    {Opcode::ROT = 0x1C,         ///< Rotate top three stack values
    {Opcode::DRAW_CIRCLE = 0x1D, ///< Draw circle at current position (radius from stack)
    {Opcode::HALT = 0xFF,        ///< Halt execution
};

// --- Implementation of argument printing functions ---

void print_no_args(const std::vector<uint8_t>&, size_t) { /* No arguments */ }

void print_reg_float(const std::vector<uint8_t>& bytecode, size_t pc) {
    uint8_t reg = bytecode[pc + 1];
    float val;
    std::memcpy(&val, &bytecode[pc + 2], sizeof(float)); // Safe type-punning
    std::cout << " r" << static_cast<int>(reg) << ", " << val;
}

void print_reg_reg(const std::vector<uint8_t>& bytecode, size_t pc) {
    uint8_t reg1 = bytecode[pc + 1];
    uint8_t reg2 = bytecode[pc + 2];
    std::cout << " r" << static_cast<int>(reg1) << ", r" << static_cast<int>(reg2);
}

void print_reg_reg_reg(const std::vector<uint8_t>& bytecode, size_t pc) {
    uint8_t r1 = bytecode[pc + 1];
    uint8_t r2 = bytecode[pc + 2];
    uint8_t r3 = bytecode[pc + 3];
    std::cout << " r" << static_cast<int>(r1) << ", r" << static_cast<int>(r2) << ", r" << static_cast<int>(r3);
}

void print_5_regs(const std::vector<uint8_t>& bytecode, size_t pc) {
    std::cout << " r" << static_cast<int>(bytecode[pc + 1]) << ", r" << static_cast<int>(bytecode[pc + 2])
              << ", r" << static_cast<int>(bytecode[pc + 3]) << ", r" << static_cast<int>(bytecode[pc + 4])
              << ", r" << static_cast<int>(bytecode[pc + 5]);
}

void disassemble(const std::vector<uint8_t>& bytecode) {
    size_t pc = 0; // Program Counter
    size_t instruction_count = 0;

    std::cout << "--- Disassembly (" << bytecode.size() << " bytes) ---\n";

    while (pc < bytecode.size()) {
        std::cout << std::setw(4) << std::setfill('0') << std::right << pc << ": ";

        Opcode op = static_cast<Opcode>(bytecode[pc]);
        auto it = opcode_map.find(op);

        if (it != opcode_map.end()) {
            const auto& info = it->second;
            std::cout << std::left << std::setw(16) << std::setfill(' ') << info.name;
            info.print_args(bytecode, pc);
            pc += info.size;
        } else {
            std::cout << "UNKNOWN (0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(op) << std::dec << ")";
            pc++; // Move to the next byte to prevent getting stuck
        }
        std::cout << '\n';
        instruction_count++;
    }

    std::cout << "--- End Disassembly (" << instruction_count << " instructions) ---\n";
}

} // namespace evosim

void printUsage(const char* progName) {
    std::cerr << "Usage: " << progName << " <bytecode_file.bin>\n";
    std::cerr << "Analyzes and disassembles a bytecode binary file.\n";
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string filepath = argv[1];
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << filepath << "'\n";
        return 1;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> bytecode(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytecode.data()), size)) {
        std::cerr << "Error: Could not read file '" << filepath << "'\n";
        return 1;
    }

    evosim::disassemble(bytecode);

    return 0;
}