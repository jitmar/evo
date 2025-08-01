#pragma once

#include <vector>
#include <memory>
#include <random>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

namespace evosim {

/**
 * @brief Virtual machine for executing organism bytecode to generate images
 * 
 * The VM interprets bytecode instructions to create visual patterns
 * that can be analyzed for symmetry.
 */
class BytecodeVM {
public:
    using Bytecode = std::vector<uint8_t>;
    using Image = cv::Mat;

    /**
     * @brief VM execution state
     */
    struct VMState {
        std::vector<uint8_t> stack;     ///< Operand stack
        std::vector<uint8_t> memory;    ///< Memory space
        uint32_t pc;                    ///< Program counter
        uint32_t x, y;                  ///< Current drawing position
        uint8_t color;                  ///< Current drawing color
        bool running;                   ///< VM running state
    };

    /**
     * @brief VM configuration
     */
    struct Config {
        uint32_t image_width;     ///< Output image width
        uint32_t image_height;    ///< Output image height
        uint32_t memory_size;     ///< VM memory size
        uint32_t stack_size;      ///< Stack size limit
        uint32_t max_instructions; ///< Maximum instructions per execution
        
        Config() : image_width(256), image_height(256), memory_size(1024),
                   stack_size(256), max_instructions(10000) {}
    };

    /**
     * @brief Bytecode instruction set
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
        SET_X = 0x14,       ///< Set X coordinate
        SET_Y = 0x15,       ///< Set Y coordinate
        SET_COLOR = 0x16,   ///< Set drawing color
        RANDOM = 0x17,      ///< Push random value to stack
        DUP = 0x18,         ///< Duplicate top stack value
        SWAP = 0x19,        ///< Swap top two stack values
        ROT = 0x1A,         ///< Rotate top three stack values
        HALT = 0xFF         ///< Halt execution
    };

    /**
     * @brief Constructor
     * @param config VM configuration
     */
    explicit BytecodeVM(const Config& config = Config());

    /**
     * @brief Destructor
     */
    ~BytecodeVM() = default;

    /**
     * @brief Execute bytecode to generate an image
     * @param bytecode Bytecode to execute
     * @return Generated image
     */
    Image execute(const Bytecode& bytecode);

    /**
     * @brief Execute bytecode with custom initial state
     * @param bytecode Bytecode to execute
     * @param initial_state Initial VM state
     * @return Generated image
     */
    Image execute(const Bytecode& bytecode, const VMState& initial_state);

    /**
     * @brief Get last execution statistics
     * @return Execution statistics
     */
    struct ExecutionStats {
        uint32_t instructions_executed;
        uint32_t pixels_drawn;
        uint32_t stack_operations;
        uint32_t memory_operations;
        bool halted_normally;
        std::string error_message;
    };

    const ExecutionStats& getLastStats() const { return last_stats_; }

    /**
     * @brief Reset VM state
     */
    void reset();

    /**
     * @brief Set VM configuration
     * @param config New configuration
     */
    void setConfig(const Config& config) { config_ = config; }

    /**
     * @brief Get VM configuration
     * @return Current configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * @brief Validate bytecode
     * @param bytecode Bytecode to validate
     * @return True if bytecode is valid
     */
    bool validateBytecode(const Bytecode& bytecode) const;

    /**
     * @brief Disassemble bytecode to human-readable format
     * @param bytecode Bytecode to disassemble
     * @return Disassembled code as string
     */
    std::string disassemble(const Bytecode& bytecode) const;

private:
    Config config_;                     ///< VM configuration
    VMState state_;                     ///< Current VM state
    Image canvas_;                      ///< Drawing canvas
    ExecutionStats last_stats_;         ///< Last execution statistics
    std::mt19937 rng_;                  ///< Random number generator

    /**
     * @brief Execute single instruction
     * @param opcode Instruction opcode
     * @param operand Instruction operand (if any)
     * @return True if execution should continue
     */
    bool executeInstruction(Opcode opcode, uint8_t operand = 0);

    /**
     * @brief Push value to stack
     * @param value Value to push
     * @return True if successful
     */
    bool pushStack(uint8_t value);

    /**
     * @brief Pop value from stack
     * @param value Output value
     * @return True if successful
     */
    bool popStack(uint8_t& value);

    /**
     * @brief Peek top stack value
     * @param value Output value
     * @return True if successful
     */
    bool peekStack(uint8_t& value) const;

    /**
     * @brief Draw pixel at current position
     */
    void drawPixel();

    /**
     * @brief Check if coordinates are within bounds
     * @param x X coordinate
     * @param y Y coordinate
     * @return True if within bounds
     */
    bool isInBounds(uint32_t x, uint32_t y) const;

    /**
     * @brief Initialize VM state
     */
    void initializeState();

    /**
     * @brief Reset execution statistics
     */
    void resetStats();
};

} // namespace evosim 