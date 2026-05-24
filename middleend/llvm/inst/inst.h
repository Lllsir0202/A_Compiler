/**
 * @brief This file defines the base instruction class
 * @note All instruction should inherit from this class,
 */
#ifndef MIDDLEEND_LLVM_INST_INST_H
#define MIDDLEEND_LLVM_INST_INST_H
#include <fstream>
#include <vector>
#include "middleend/llvm/inst/opkind.h"
// Some forward declaration
namespace middleend::llvm::utils {
class BasicBlock;
class Operand;
}  // namespace middleend::llvm::utils

namespace middleend::llvm::utils::factory {
class InstFactory;
}  // namespace middleend::llvm::utils::factory

namespace context {
class CompilerContext;
class LLVMIRContext;
}  // namespace context

namespace middleend::llvm::inst {
class Instruction {
 public:
  virtual auto Emit(std::ofstream *out) const -> void = 0;
  // Interface to get and set the parent block
  auto GetParentBlock() const -> utils::BasicBlock * { return parent_block_; }
  auto SetParentBlock(utils::BasicBlock *block) -> void { parent_block_ = block; }

  // Interface to get and set the operands
  auto AddOperand(utils::Operand *operand) -> void { operands_.emplace_back(operand); }
  auto GetOperands() const -> const std::vector<utils::Operand *> & { return operands_; }

  // Interface to get and set the result
  auto GetResult() const -> utils::Operand * { return result_; }
  auto SetResult(utils::Operand *result) -> void { result_ = result; }

  // Then we provide the interface to get the users of the instruction's result
  auto GetUsers() const -> const std::vector<Instruction *> & { return users_; }
  auto AddUser(Instruction *user) -> void { users_.emplace_back(user); }

  // Provide interface to replace all the users with a new operand
  auto ReplaceAllUsersWith(utils::Operand *new_operand) -> void {
    for (auto *user : users_) {
      for (auto &operand : user->operands_) {
        if (operand == result_) {
          operand = new_operand;
        }
      }
    }
    // Clear the users after replacement
    users_.clear();
  }

  // Interface to get the opkind
  auto GetOpKind() const -> OpKind { return op_kind_; }
  auto VerifyOpKind(OpKind expected_op_kind) const -> bool { return op_kind_ == expected_op_kind; }

  // Virtual destructor to ensure proper cleanup of derived classes
  virtual ~Instruction() = default;

  // Virtual function to verify the instruction
  virtual auto Verify(context::CompilerContext *context) const -> bool = 0;

 protected:
  explicit Instruction(OpKind op_kind) : op_kind_(op_kind) {}

  // Opkind
  OpKind op_kind_;

  // Record parent block
  utils::BasicBlock *parent_block_ = nullptr;
  // Record the operands of the instruction
  std::vector<utils::Operand *> operands_;

  // Record the users of the instruction's result,
  // and currently we only support one result for each instruction
  utils::Operand *result_ = nullptr;
  std::vector<Instruction *> users_;
};

template <typename InstType>
auto DynamicOrNull(Instruction *inst) {
  return dynamic_cast<InstType *>(inst);
}

}  // namespace middleend::llvm::inst

#endif