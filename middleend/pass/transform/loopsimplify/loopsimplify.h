#ifndef MIDDLEEND_PASS_TRANSFORM_LOOPSIMPLIFY_LOOPSIMPLIFY_H
#define MIDDLEEND_PASS_TRANSFORM_LOOPSIMPLIFY_LOOPSIMPLIFY_H

#include <stdexcept>
#include <unordered_map>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
}  // namespace middleend::mlir

namespace middleend::mlir::base {
class FuncDefOp;
}  // namespace middleend::mlir::base

namespace middleend::pass::analysis {
class Loop;
class LoopInfoPass;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Ensures every natural loop is in LoopSimplify form with respect to its preheader: a
 * single dedicated predecessor of the header that lies outside the loop and whose only successor
 * is the header. LICM hoists into this block.
 *
 * Structured SysY lowering already emits such a preheader for every loop (the block that
 * unconditionally branches into the loop-condition block), so in practice this pass verifies the
 * property and does nothing. When a header instead has several outside predecessors, or a single
 * outside predecessor that also branches elsewhere, a fresh preheader is inserted: the outside
 * edges are redirected through it and the header's phi incomings from those edges are merged into
 * a phi in the new block, preserving SSA.
 *
 * It also puts single-exit loops into **LCSSA form**: a value defined inside the loop and used
 * after it is routed through a phi in the loop's exit block. Consumers then see every loop-carried
 * result at one well-known place instead of reaching into the loop body, which is what lets
 * DeadLoopElim reason about live-outs without matching on the loop's shape, and what keeps loop
 * rotation's SSA repair a plain two-way merge. Exit blocks are already dedicated in practice (the
 * structured SysY lowering emits one per loop), so no exit splitting is needed here.
 */
class LoopSimplifyPass : public TransformPass {
 public:
  LoopSimplifyPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("LoopSimplifyPass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("LoopInfoPass");
  }
  ~LoopSimplifyPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "LoopSimplifyPass"; }

 private:
  auto EnsurePreheader(mlir::base::FuncDefOp *func_op, analysis::Loop *loop) -> bool;
  auto OutsidePredecessors(mlir::base::FuncDefOp *func_op, analysis::Loop *loop) -> std::vector<mlir::Block *>;
  auto HasDedicatedPreheader(const std::vector<mlir::Block *> &outside, mlir::Block *header) -> bool;
  auto InsertPreheader(mlir::base::FuncDefOp *func_op, analysis::Loop *loop, const std::vector<mlir::Block *> &outside)
      -> void;
  auto RedirectTerminator(mlir::Block *block, mlir::Block *from, mlir::Block *to) -> void;
  // Route every in-loop definition that is used after the loop through an exit-block phi.
  auto FormLCSSA(mlir::base::FuncDefOp *func_op, analysis::Loop *loop) -> bool;

  // Block each operation lives in, rebuilt per function; used to classify a use as in- or
  // out-of-loop. Kept up to date for the phis FormLCSSA inserts.
  std::unordered_map<mlir::Operation *, mlir::Block *> op_block_;
};

}  // namespace middleend::pass::transform

#endif
