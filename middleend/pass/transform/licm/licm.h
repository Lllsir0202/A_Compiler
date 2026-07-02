#ifndef MIDDLEEND_PASS_TRANSFORM_LICM_LICM_H
#define MIDDLEEND_PASS_TRANSFORM_LICM_LICM_H

#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::pass::analysis {
class AliasAnalysisPass;
class Loop;
class LoopInfoPass;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Loop-invariant code motion for BASEIR, hoisting into each loop's preheader.
 *
 * Scalar LICM hoists a speculatable pure operation (integer/float arithmetic and comparisons,
 * unary ops, casts, constants, and `var.getptr` address computation) whose operands are all
 * loop-invariant. Potentially trapping integer div/mod are excluded, so a hoisted op is always
 * safe to run once in the preheader even when the loop body would not have executed it.
 *
 * Memory LICM hoists a `var.getval` load whose address is loop-invariant and which no store or
 * mod/ref call inside the loop may alias — the loaded value is then the same on every iteration.
 * A load has no side effect, so moving it to the preheader is safe even for a zero-trip loop.
 *
 * Hoisting relocates operations between existing blocks only; it never edits terminators, so the
 * CFG, dominator tree and loop forest stay valid across the whole pass. A fixpoint over each loop
 * lets a hoisted address feed a subsequently hoisted load, and inner loops are processed before
 * the loops that enclose them.
 */
class LICMPass : public TransformPass {
 public:
  LICMPass(context::CompilerContext *compiler_context, PassStage stage) : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("LICMPass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("LoopInfoPass");
    AddDependency("AliasAnalysisPass");
    AddDependency("LoopSimplifyPass");
  }
  ~LICMPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "LICMPass"; }

 private:
  auto RunOnLoop(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> bool;
  auto FindPreheader(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> mlir::Block *;
  auto IsInvariant(mlir::Value *value, analysis::Loop *loop) -> bool;
  auto IsHoistable(mlir::Operation *operation, analysis::Loop *loop, const std::vector<mlir::Operation *> &loop_stores,
                   const std::vector<mlir::Operation *> &loop_calls) -> bool;
  auto IsSpeculatablePure(mlir::Operation *operation) -> bool;
  auto Hoist(mlir::Operation *operation, mlir::Block *preheader) -> void;

  analysis::AliasAnalysisPass *alias_analysis_{nullptr};
  std::unordered_map<mlir::Operation *, mlir::Block *> op_to_block_;
};

}  // namespace middleend::pass::transform

#endif
