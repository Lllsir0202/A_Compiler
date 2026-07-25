#ifndef MIDDLEEND_PASS_TRANSFORM_LOOPROTATE_LOOPROTATE_H
#define MIDDLEEND_PASS_TRANSFORM_LOOPROTATE_LOOPROTATE_H

#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
}

namespace middleend::mlir::base {
class FuncDefOp;
}

namespace middleend::pass::analysis {
class Loop;
class LoopInfoPass;
class CFGPass;
class MLIRFunctionCFG;
}  // namespace middleend::pass::analysis

namespace middleend::pass::transform {

/**
 * @brief Loop rotation: turn a top-tested `while (c) { body }` loop into a
 * guarded do-while `if (c) { do { body } while (c); }` so that the loop body
 * dominates the back-edge test. This gives LICM/Promotion a body that is
 * guaranteed to execute, exposing more invariant code and enabling reduction
 * register promotion.
 *
 * At the BASEIR level the canonical loop after LoopSimplify is
 *   preheader -> header(phi; cond; cond_br body/exit) ; body...latch -> header
 * Rotation clones the header's condition computation into the preheader as a
 * guard, drops the preheader->header edge (the preheader now jumps straight
 * into the body through a fresh dedicated preheader), and reconstructs SSA for
 * every value the old header defined: uses inside the loop read a merge phi in
 * the body, uses after the loop read a merge phi in the (dedicated) exit.
 *
 * Only loops whose sole exit edge is the header test are rotated (loops with
 * `break` have several exits and are left untouched); the header must consist
 * of phis followed by safely clonable operations. Nested loops are handled
 * innermost-first. Scalar-only loops stay top-tested so SCEV can prefer a
 * closed form over control-flow tuning.
 *
 * Reference: LLVM LoopRotate.
 */
class LoopRotatePass : public TransformPass {
 public:
  LoopRotatePass(context::CompilerContext *compiler_context, PassStage stage) : TransformPass(compiler_context, stage) {
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("LoopInfoPass");
    AddDependency("LoopSimplifyPass");
  }
  ~LoopRotatePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "LoopRotatePass"; }

 private:
  // Rotate one loop in place; returns true when the loop was transformed.
  auto TryRotate(mlir::base::FuncDefOp *func_op, analysis::Loop *loop) -> bool;
};

}  // namespace middleend::pass::transform

#endif
