#include "test_common.h"
#include "core/bytecode_vm.h"

namespace evosim {

class BytecodeVMTest : public ::testing::Test {
protected:
    void SetUp() override {
        BytecodeVM::Config config;
        config.memory_size = 1024;
        config.stack_size = 256;
        config.max_instructions = 10000;
        vm_ = std::make_unique<BytecodeVM>(config);
    }
    
    void TearDown() override {
        vm_.reset();
    }
    
    std::unique_ptr<BytecodeVM> vm_;
};

TEST_F(BytecodeVMTest, Constructor) {
    EXPECT_NE(vm_, nullptr);
}

TEST_F(BytecodeVMTest, LoadEmptyBytecode) {
    BytecodeVM::Bytecode empty_bytecode;
    auto image = vm_->execute(empty_bytecode);
    // Empty bytecode should still produce a blank canvas
    EXPECT_FALSE(image.empty());
    EXPECT_EQ(image.rows, 256);
    EXPECT_EQ(image.cols, 256);
}

TEST_F(BytecodeVMTest, LoadValidBytecode) {
    BytecodeVM::Bytecode bytecode = {0x01, 0x42, 0xFF}; // PUSH 0x42, HALT
    auto image = vm_->execute(bytecode);
    EXPECT_FALSE(image.empty());
}

TEST_F(BytecodeVMTest, GenerateImage) {
    BytecodeVM::Bytecode bytecode = {0x01, 0x42, 0xFF}; // PUSH 0x42, HALT
    auto image = vm_->execute(bytecode);
    EXPECT_EQ(image.rows, 256); // Default height from config
    EXPECT_EQ(image.cols, 256); // Default width from config
    EXPECT_EQ(image.type(), CV_8UC3); // 3-channel RGB
}

TEST_F(BytecodeVMTest, Reset) {
    BytecodeVM::Bytecode bytecode = {0x01, 0x42, 0xFF}; // PUSH 0x42, HALT
    auto image1 = vm_->execute(bytecode);
    
    vm_->reset();
    
    // After reset, should be able to execute new bytecode
    BytecodeVM::Bytecode new_bytecode = {0x01, 0x55, 0xFF}; // PUSH 0x55, HALT
    auto image2 = vm_->execute(new_bytecode);
    EXPECT_FALSE(image2.empty());
}

TEST_F(BytecodeVMTest, ExecuteInstructions) {
    BytecodeVM::Bytecode bytecode = {0x01, 0x42, 0xFF}; // PUSH 0x42, HALT
    auto image = vm_->execute(bytecode);
    EXPECT_FALSE(image.empty());
}

TEST_F(BytecodeVMTest, Disassemble) {
    BytecodeVM::Bytecode bytecode = {0x01, 0x42, 0xFF}; // PUSH 0x42, HALT
    std::string disassembly = vm_->disassemble(bytecode);
    
    EXPECT_FALSE(disassembly.empty());
    EXPECT_NE(disassembly.find("PUSH"), std::string::npos);
    EXPECT_NE(disassembly.find("HALT"), std::string::npos);
}

TEST_F(BytecodeVMTest, OpcodeAdd) {
    // PUSH 10, PUSH 20, ADD, HALT
    BytecodeVM::Bytecode bytecode = {0x01, 10, 0x01, 20, 0x03, 0xFF};
    vm_->execute(bytecode);
    const auto& state = vm_->getLastState();
    ASSERT_EQ(state.stack.size(), 1);
    uint8_t result = state.stack.back();
    EXPECT_EQ(result, 30);
}

TEST_F(BytecodeVMTest, OpcodeSub) {
    // PUSH 20, PUSH 10, SUB, HALT
    BytecodeVM::Bytecode bytecode = {0x01, 20, 0x01, 10, 0x04, 0xFF};
    vm_->execute(bytecode);
    const auto& state = vm_->getLastState();
    ASSERT_EQ(state.stack.size(), 1);
    uint8_t result = state.stack.back();
    EXPECT_EQ(result, 10);
}

TEST_F(BytecodeVMTest, OpcodeDivByZero) {
    // PUSH 10, PUSH 0, DIV, HALT
    BytecodeVM::Bytecode bytecode = {0x01, 10, 0x01, 0, 0x06, 0xFF};
    vm_->execute(bytecode);
    auto stats = vm_->getLastStats();
    EXPECT_FALSE(stats.halted_normally);
    EXPECT_EQ(stats.error_message, "Division by zero");
}

TEST_F(BytecodeVMTest, OpcodeJmp) {
    // JMP to address 4, which contains HALT. The PUSH at address 2 should be skipped.
    BytecodeVM::Bytecode bytecode = {0x0C, 4, 0x01, 1, 0xFF};
    vm_->execute(bytecode);
    EXPECT_TRUE(vm_->getLastState().stack.empty());
}

} // namespace evosim 