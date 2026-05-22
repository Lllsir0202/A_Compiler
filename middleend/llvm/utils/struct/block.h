#ifndef MIDDLEEND_LLVM_UTILS_STRUCT_BLOCK_H
#define MIDDLEEND_LLVM_UTILS_STRUCT_BLOCK_H

#include <cstddef>
#include <deque>
#include <fstream>
#include <string>
#include <unordered_set>
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
  ~BasicBlock();
  // Interface for basic block to get block information
  auto GetID() const -> size_t { return id_; }
  auto GetInstructions() const -> std::deque<inst::Instruction *> { return instructions_; }
  auto SetInstructions(const std::deque<inst::Instruction *> &instructions) -> void { instructions_ = instructions; }
  auto AddInstBack(inst::Instruction *inst) -> void { instructions_.emplace_back(inst); }
  auto AddInstFront(inst::Instruction *inst) -> void { instructions_.emplace_front(inst); }
  auto RemoveInst(inst::Instruction *inst) -> void {
    std::deque<inst::Instruction *> new_insts;
    for (auto *i : instructions_) {
      if (i != inst) {
        new_insts.emplace_back(i);
      }
    }
    instructions_ = std::move(new_insts);
  }

  auto RemoveInsts(std::unordered_set<inst::Instruction *> &insts) -> void {
    std::deque<inst::Instruction *> new_insts;
    for (auto *i : instructions_) {
      if (insts.count(i) == 0) {
        new_insts.emplace_back(i);
      }
    }
    instructions_ = std::move(new_insts);
  }

  // Interface for debug, comment
  auto GetComment() const -> const std::string & { return comment_; }
  auto SetComment(const std::string &comment) -> void { comment_ = comment; }

  auto Emit(std::ofstream *out) const -> void;

 private:
  explicit BasicBlock(size_t id) : id_(id) {}
  const size_t id_;
  // Used for debug, comment
  std::string comment_;

  std::deque<inst::Instruction *> instructions_;
  friend class factory::BlockFactory;
};

}  // namespace middleend::llvm::utils

#endif