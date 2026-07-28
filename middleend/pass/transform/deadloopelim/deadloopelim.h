#ifndef MIDDLEEND_PASS_TRANSFORM_DEADLOOPELIM_DEADLOOPELIM_H
#define MIDDLEEND_PASS_TRANSFORM_DEADLOOPELIM_DEADLOOPELIM_H

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir
namespace middleend::mlir::base {
class FuncDefOp;
}
namespace middleend::pass::analysis {
class Loop;
class LoopInfoPass;
class MLIRFunctionCFG;
class SCEVPass;
class SCEV;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Delete a loop whose only observable effect is the final value of its induction variables.
 *
 * A loop is removed when it is side-effect free (no stores and no calls), top-tested with a single
 * exit, and has a compile-time-constant trip count, and when every value it leaves live can be
 * expressed in closed form. Each such live-out is a header recurrence, so its exit value is the
 * scalar-evolution expression evaluated at the trip count (a running sum like `sum += i` closes to
 * n*(n-1)/2). The closed forms are materialized in the preheader, every out-of-loop use is rewired
 * to them, the preheader is branched straight to the exit, and the loop blocks are dropped.
 *
 * This turns an O(n) accumulator loop that was too large to unroll into O(1). It only fires when the
 * transform is unambiguous and legal, and aborts (touching nothing) at the first doubt, so it never
 * needs a no-wrap assumption beyond the one already isolated in the trip-count computation. After a
 * deletion the analyses are invalidated, so the passes that follow rebuild the CFG, dominator tree
 * and loop forest from the mutated IR.
 */
class DeadLoopEliminatePass : public TransformPass {
 public:
  DeadLoopEliminatePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("DeadLoopEliminatePass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("LoopInfoPass");
    AddDependency("LoopSimplifyPass");
    AddDependency("SCEVPass");
  }
  ~DeadLoopEliminatePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "DeadLoopEliminatePass"; }

 private:
  auto TryDeleteLoop(analysis::MLIRFunctionCFG *function_cfg, mlir::base::FuncDefOp *func_op,
                     analysis::Loop *loop) -> bool;
  auto FindPreheader(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> mlir::Block *;

  // The single block outside `loop` that the header branches to on its exit edge, or nullptr when the
  // loop is not a clean single-exit, top-tested loop.
  auto FindUniqueExit(analysis::Loop *loop, mlir::Block **exiting_out) -> mlir::Block *;
  auto HasSideEffect(analysis::Loop *loop) -> bool;

  // Materialize a loop-invariant closed-form SCEV before the terminator of `at`; nullptr if it cannot
  // be expanded there (e.g. it still contains a recurrence).
  auto Expand(const analysis::SCEV *scev, mlir::Block *at) -> mlir::Value *;
  auto MakeConstInt(int value, mlir::Block *at) -> mlir::Value *;
  template <typename BinaryOp>
  auto MakeBinary(mlir::Value *lhs, mlir::Value *rhs, mlir::Block *at) -> mlir::Value *;
  template <typename CompareOp>
  auto MakeComparison(mlir::Value *lhs, mlir::Value *rhs, mlir::Block *at) -> mlir::Value *;
  auto MakeBoolToInt(mlir::Value *value, mlir::Block *at) -> mlir::Value *;
  auto ExpandRepeatedDiv(const analysis::SCEV *scev, mlir::Block *at) -> mlir::Value *;
  auto ExpandModAddRec(const analysis::SCEV *scev, mlir::Block *at) -> mlir::Value *;
  auto InsertBeforeTerminator(mlir::Operation *operation, mlir::Block *block) -> void;

  analysis::SCEVPass *scev_{nullptr};
  std::unordered_map<mlir::Operation *, mlir::Block *> op_to_block_;
  int deleted_count_{0};
};

}  // namespace middleend::pass::transform

#endif
