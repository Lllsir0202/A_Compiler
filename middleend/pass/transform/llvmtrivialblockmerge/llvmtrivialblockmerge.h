#ifndef MIDDLEEND_PASS_TRANSFORM_LLVMTRIVIALBLOCKMERGE_LLVMTRIVIALBLOCKMERGE_H
#define MIDDLEEND_PASS_TRANSFORM_LLVMTRIVIALBLOCKMERGE_LLVMTRIVIALBLOCKMERGE_H

#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include "middleend/pass/transform/transform.h"

namespace middleend::llvm::inst {
class Instruction;
}  // namespace middleend::llvm::inst

namespace middleend::llvm::utils {
class BasicBlock;
class Function;
}  // namespace middleend::llvm::utils

namespace middleend::pass::transform {
class LLVMIRTrivialBlockMergePass : public TransformPass {
 public:
  LLVMIRTrivialBlockMergePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::LLVMIR) {
      throw std::invalid_argument("LLVMIRTrivialBlockMergePass only supports LLVMIR stage");
    }
  }
  ~LLVMIRTrivialBlockMergePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "LLVMIRTrivialBlockMergePass"; }

 private:
  auto MergeInFunction(llvm::utils::Function *function) -> bool;
  auto BuildPredecessors(llvm::utils::Function *function)
      -> std::unordered_map<llvm::utils::BasicBlock *, std::unordered_set<llvm::utils::BasicBlock *>>;
  auto TryMergeEntryBlock(
      llvm::utils::Function *function,
      const std::unordered_map<llvm::utils::BasicBlock *, std::unordered_set<llvm::utils::BasicBlock *>> &predecessors)
      -> bool;
  auto IsMergeableBlock(llvm::utils::Function *function, llvm::utils::BasicBlock *block) -> bool;
  auto CanMergeBlock(
      llvm::utils::Function *function, llvm::utils::BasicBlock *block,
      const std::unordered_map<llvm::utils::BasicBlock *, std::unordered_set<llvm::utils::BasicBlock *>> &predecessors)
      -> bool;
  auto HasPhi(llvm::utils::BasicBlock *block) -> bool;
  auto HasPhiIncomingFrom(llvm::utils::BasicBlock *block, llvm::utils::BasicBlock *incoming_block) -> bool;
  auto RewriteBranchTarget(llvm::inst::Instruction *instruction, llvm::utils::BasicBlock *old_target,
                           llvm::utils::BasicBlock *new_target) -> bool;
  auto RewritePhiIncoming(llvm::utils::BasicBlock *target, llvm::utils::BasicBlock *old_block,
                          llvm::utils::BasicBlock *new_block) -> void;
  auto RewriteSuccessorPhiIncoming(llvm::utils::Function *function, llvm::utils::BasicBlock *block,
                                   llvm::utils::BasicBlock *old_block, llvm::utils::BasicBlock *new_block) -> void;
  auto RemoveBlock(llvm::utils::Function *function, llvm::utils::BasicBlock *block_to_remove) -> void;
  auto RemoveMergedBlocks(llvm::utils::Function *function) -> void;
  auto GetBlockByID(llvm::utils::Function *function, size_t block_id) -> llvm::utils::BasicBlock *;
};

}  // namespace middleend::pass::transform

#endif
