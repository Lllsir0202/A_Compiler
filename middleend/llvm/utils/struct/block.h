#ifndef MIDDLEEND_LLVM_UTILS_STRUCT_BLOCK_H
#define MIDDLEEND_LLVM_UTILS_STRUCT_BLOCK_H

#include <cstddef>
#include <deque>
namespace middleend::llvm::inst {
class Instruction;
}
namespace middleend::llvm::utils::factory {
// Forward declare the block factory to allow it to create blocks
class BlockFactory;
}  // namespace middleend::llvm::utils::factory

namespace middleend::llvm::utils {
/**
 * @brief This class defines the basic block for llvmir,
 * and it should contain a list of instructions, and a unique block id,
 */
class BasicBlock {
 public:
  // Interface for basic block to get block information
  auto GetID() const -> size_t { return id_; }
  auto GetInstructions() const -> const std::deque<inst::Instruction *> & { return instructions_; }
  auto SetInstructions(const std::deque<inst::Instruction *> &instructions) -> void { instructions_ = instructions; }
  auto AddInstBack(inst::Instruction *inst) -> void { instructions_.emplace_back(inst); }
  auto AddInstFront(inst::Instruction *inst) -> void { instructions_.emplace_front(inst); }

 private:
  explicit BasicBlock(size_t id) : id_(id) {}
  const size_t id_;

  std::deque<inst::Instruction *> instructions_;
  friend class factory::BlockFactory;
};

}  // namespace middleend::llvm::utils

#endif