#ifndef MIDDLEEND_PASS_TRANSFORM_TRIVIALBLOCKMERGE_TRIVIALBLOCKMERGE_H
#define MIDDLEEND_PASS_TRANSFORM_TRIVIALBLOCKMERGE_TRIVIALBLOCKMERGE_H

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
}  // namespace middleend::mlir

namespace middleend::mlir::base {
class FuncDefOp;
}  // namespace middleend::mlir::base

namespace middleend::pass::transform {
class TrivialBlockMergePass : public TransformPass {
 public:
  TrivialBlockMergePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("TrivialBlockMergePass only supports BASEIR stage");
    }
    AddDependency("PostSCCPADCEPass");
  }
  ~TrivialBlockMergePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "TrivialBlockMergePass"; }

 private:
  auto MergeInFunction(mlir::base::FuncDefOp *func_op) -> bool;
  auto BuildPredecessors(mlir::base::FuncDefOp *func_op)
      -> std::unordered_map<mlir::Block *, std::unordered_set<mlir::Block *>>;
  auto IsMergeableBlock(mlir::base::FuncDefOp *func_op, mlir::Block *block) -> bool;
  auto CanMergeBlock(mlir::Block *block,
                     const std::unordered_map<mlir::Block *, std::unordered_set<mlir::Block *>> &predecessors) -> bool;
  auto HasPhi(mlir::Block *block) -> bool;
  auto HasPhiIncomingFrom(mlir::Block *block, mlir::Block *incoming_block) -> bool;
  auto RewriteBranchTarget(mlir::Operation *operation, mlir::Block *old_target, mlir::Block *new_target) -> bool;
  auto RewritePhiIncoming(mlir::Block *target, mlir::Block *old_block, mlir::Block *new_block) -> void;
  auto RemoveMergedBlocks(mlir::base::FuncDefOp *func_op) -> void;
};

}  // namespace middleend::pass::transform

#endif
