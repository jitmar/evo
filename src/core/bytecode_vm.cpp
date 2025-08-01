#include "core/bytecode_vm.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace evosim {

BytecodeVM::BytecodeVM(const Config& config)
    : config_(config)
    , rng_(std::random_device{}()) {
    initializeState();
}

BytecodeVM::Image BytecodeVM::execute(const Bytecode& bytecode) {
    reset();
    
    // Copy bytecode to memory
    size_t copy_size = std::min(bytecode.size(), static_cast<size_t>(config_.memory_size));
    std::copy(bytecode.begin(), bytecode.begin() + copy_size, state_.memory.begin());
    
    // Execute bytecode
    while (state_.running && state_.pc < state_.memory.size() && 
           last_stats_.instructions_executed < config_.max_instructions) {
        
        uint8_t opcode = state_.memory[state_.pc];
        uint8_t operand = (state_.pc + 1 < state_.memory.size()) ? state_.memory[state_.pc + 1] : 0;
        
        if (!executeInstruction(static_cast<Opcode>(opcode), operand)) {
            break;
        }
        
        last_stats_.instructions_executed++;
    }
    
    last_stats_.halted_normally = !state_.running;
    return canvas_.clone();
}

BytecodeVM::Image BytecodeVM::execute(const Bytecode& bytecode, const VMState& initial_state) {
    state_ = initial_state;
    canvas_ = cv::Mat::zeros(config_.image_height, config_.image_width, CV_8UC3);
    resetStats();
    
    // Copy bytecode to memory
    size_t copy_size = std::min(bytecode.size(), static_cast<size_t>(config_.memory_size));
    std::copy(bytecode.begin(), bytecode.begin() + copy_size, state_.memory.begin());
    
    // Execute bytecode
    while (state_.running && state_.pc < state_.memory.size() && 
           last_stats_.instructions_executed < config_.max_instructions) {
        
        uint8_t opcode = state_.memory[state_.pc];
        uint8_t operand = (state_.pc + 1 < state_.memory.size()) ? state_.memory[state_.pc + 1] : 0;
        
        if (!executeInstruction(static_cast<Opcode>(opcode), operand)) {
            break;
        }
        
        last_stats_.instructions_executed++;
    }
    
    last_stats_.halted_normally = !state_.running;
    return canvas_.clone();
}

void BytecodeVM::reset() {
    initializeState();
    resetStats();
}



bool BytecodeVM::validateBytecode(const Bytecode& bytecode) const {
    if (bytecode.empty()) {
        return false;
    }
    
    for (size_t i = 0; i < bytecode.size(); ++i) {
        uint8_t opcode = bytecode[i];
        
        // Check if opcode is valid
        if (opcode > static_cast<uint8_t>(Opcode::HALT)) {
            return false;
        }
        
        // Check for operand requirements
        switch (static_cast<Opcode>(opcode)) {
            case Opcode::PUSH:
            case Opcode::JMP:
            case Opcode::JZ:
            case Opcode::JNZ:
            case Opcode::CALL:
            case Opcode::LOAD:
            case Opcode::STORE:
            case Opcode::SET_X:
            case Opcode::SET_Y:
            case Opcode::SET_COLOR:
                if (i + 1 >= bytecode.size()) {
                    return false; // Missing operand
                }
                i++; // Skip operand
                break;
            default:
                break;
        }
    }
    
    return true;
}

std::string BytecodeVM::disassemble(const Bytecode& bytecode) const {
    std::ostringstream oss;
    oss << "Disassembly:\n";
    
    for (size_t i = 0; i < bytecode.size(); ++i) {
        oss << std::hex << std::setw(4) << std::setfill('0') << i << ": ";
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(bytecode[i]) << " ";
        
        // Disassembly
        switch (static_cast<Opcode>(bytecode[i])) {
            case Opcode::NOP: oss << "NOP"; break;
            case Opcode::PUSH: 
                oss << "PUSH";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::POP: oss << "POP"; break;
            case Opcode::ADD: oss << "ADD"; break;
            case Opcode::SUB: oss << "SUB"; break;
            case Opcode::MUL: oss << "MUL"; break;
            case Opcode::DIV: oss << "DIV"; break;
            case Opcode::MOD: oss << "MOD"; break;
            case Opcode::AND: oss << "AND"; break;
            case Opcode::OR: oss << "OR"; break;
            case Opcode::XOR: oss << "XOR"; break;
            case Opcode::NOT: oss << "NOT"; break;
            case Opcode::JMP: 
                oss << "JMP";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::JZ: 
                oss << "JZ";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::JNZ: 
                oss << "JNZ";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::CALL: 
                oss << "CALL";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::RET: oss << "RET"; break;
            case Opcode::LOAD: 
                oss << "LOAD";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::STORE: 
                oss << "STORE";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::DRAW_PIXEL: oss << "DRAW_PIXEL"; break;
            case Opcode::SET_X: 
                oss << "SET_X";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::SET_Y: 
                oss << "SET_Y";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::SET_COLOR: 
                oss << "SET_COLOR";
                if (i + 1 < bytecode.size()) {
                    oss << " " << static_cast<int>(bytecode[i + 1]);
                    i++;
                }
                break;
            case Opcode::RANDOM: oss << "RANDOM"; break;
            case Opcode::DUP: oss << "DUP"; break;
            case Opcode::SWAP: oss << "SWAP"; break;
            case Opcode::ROT: oss << "ROT"; break;
            case Opcode::HALT: oss << "HALT"; break;
            default: oss << "UNKNOWN"; break;
        }
        
        oss << "\n";
    }
    
    return oss.str();
}

bool BytecodeVM::executeInstruction(Opcode opcode, uint8_t operand) {
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
            uint8_t a, b;
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(a + b)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::SUB:
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(a - b)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::MUL:
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(a * b)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::DIV:
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (b == 0) {
                last_stats_.error_message = "Division by zero";
                return false;
            }
            if (!pushStack(a / b)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::MOD:
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (b == 0) {
                last_stats_.error_message = "Modulo by zero";
                return false;
            }
            if (!pushStack(a % b)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::AND:
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(a & b)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::OR:
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(a | b)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::XOR:
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(a ^ b)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::NOT:
            if (!popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(~a)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::JMP:
            state_.pc = operand;
            return true;
            
        case Opcode::JZ:
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
            
        case Opcode::SET_COLOR:
            state_.color = operand;
            state_.pc += 2;
            return true;
            
        case Opcode::RANDOM:
            if (!pushStack(rng_() % 256)) {
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
            if (!popStack(b) || !popStack(a)) {
                last_stats_.error_message = "Stack underflow";
                return false;
            }
            if (!pushStack(b) || !pushStack(a)) {
                last_stats_.error_message = "Stack overflow";
                return false;
            }
            state_.pc++;
            last_stats_.stack_operations++;
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
            last_stats_.stack_operations++;
            return true;
            
        case Opcode::HALT:
            state_.running = false;
            return false;
            
        default:
            last_stats_.error_message = "Unknown opcode: " + std::to_string(static_cast<int>(opcode));
            return false;
    }
}

bool BytecodeVM::pushStack(uint8_t value) {
    if (state_.stack.size() >= config_.stack_size) {
        return false;
    }
    state_.stack.push_back(value);
    return true;
}

bool BytecodeVM::popStack(uint8_t& value) {
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

void BytecodeVM::drawPixel() {
    if (isInBounds(state_.x, state_.y)) {
        cv::Vec3b& pixel = canvas_.at<cv::Vec3b>(state_.y, state_.x);
        pixel[0] = state_.color; // Blue
        pixel[1] = state_.color; // Green
        pixel[2] = state_.color; // Red
    }
}

bool BytecodeVM::isInBounds(uint32_t x, uint32_t y) const {
    return x < config_.image_width && y < config_.image_height;
}

void BytecodeVM::initializeState() {
    state_.stack.clear();
    state_.memory.resize(config_.memory_size, 0);
    state_.pc = 0;
    state_.x = 0;
    state_.y = 0;
    state_.color = 0;
    state_.running = true;
    
    canvas_ = cv::Mat::zeros(config_.image_height, config_.image_width, CV_8UC3);
    resetStats();
}

void BytecodeVM::resetStats() {
    last_stats_.instructions_executed = 0;
    last_stats_.pixels_drawn = 0;
    last_stats_.stack_operations = 0;
    last_stats_.memory_operations = 0;
    last_stats_.halted_normally = false;
    last_stats_.error_message.clear();
}

} // namespace evosim 