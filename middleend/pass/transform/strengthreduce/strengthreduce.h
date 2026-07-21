#ifndef MIDDLEEND_PASS_TRANSFORM_STRENGTHREDUCE_STRENGTHREDUCE_H
#define MIDDLEEND_PASS_TRANSFORM_STRENGTHREDUCE_STRENGTHREDUCE_H

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace type {
class Type;
}

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir
namespace middleend::mlir::base {
class MulOp;
}
namespace middleend::mlir::var {
class VarGetPtrOp;
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
 * @brief Induction-variable strength reduction driven by scalar evolution.
 *
 * A loop-variant multiply whose value evolves as an affine recurrence {start, +, step} of the loop
 * is replaced by an additive induction variable: `start` and `step` are materialized once in the
 * preheader, a header phi carries the running value, and the latch advances it by `step`. Every use
 * inside the loop then reads the phi instead of recomputing the multiply, trading one multiply per
 * iteration for one addition.
 *
 * The rewrite is exact under 32-bit modular arithmetic -- (i+1)*c equals i*c + c for every i -- so
 * it needs no no-wrap assumption. It fires only when the transform is unambiguous and legal: the
 * loop has a dedicated preheader and a single latch, the header's only predecessors are those two
 * blocks, the recurrence belongs to this loop, `start`/`step` are loop-invariant and contain no
 * nested recurrence (so they expand into the preheader without needing an outer induction value),
 * and every use of the multiply lies inside the loop (so replacing it never violates dominance).
 *
 * Only non-terminator operations are inserted and only uses are rewired, so the CFG, dominator tree
 * and loop forest stay valid for the passes that run afterwards.
 */
class StrengthReducePass : public TransformPass {
 public:
  StrengthReducePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("StrengthReducePass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
    AddDependency("DomTreePass");
    AddDependency("LoopInfoPass");
    AddDependency("LoopSimplifyPass");
    AddDependency("SCEVPass");
  }
  ~StrengthReducePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "StrengthReducePass"; }

 private:
  // The three blocks a reduction inserts into: the recurrence starts in the preheader, advances in
  // the latch, and merges through a phi in the header.
  struct LoopFrame {
    mlir::Block *preheader;
    mlir::Block *latch;
    mlir::Block *header;
  };

  // One canonical induction variable shared by every recurrence with the same step (and, for
  // pointers, the same base and leading indices). A member that differs only by a loop-invariant
  // offset reuses it: `canonical` is the phi, `anchor` is the start it was built at.
  struct IVFamily {
    mlir::Value *canonical;
    const analysis::SCEV *anchor;
  };

  auto RunOnLoop(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> bool;
  auto FindPreheader(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> mlir::Block *;

  // Replace a loop-variant multiply (whose value is an affine recurrence of the loop) with an
  // additive induction variable, reusing one per step across recurrences that differ by an offset.
  auto ReduceMul(mlir::base::MulOp *mul, analysis::Loop *loop, const LoopFrame &frame) -> bool;
  // Replace a scalar-element `getptr` with exactly one affine varying index by
  // a pointer induction variable, reusing one per (base, invariant indices,
  // dimension, step).
  auto ReduceGetPtr(mlir::var::VarGetPtrOp *get_ptr, analysis::Loop *loop, const LoopFrame &frame) -> bool;

  // Build the canonical additive / walking-pointer induction variable: preheader init, header phi,
  // latch step. Returns the phi value.
  auto EmitScalarIV(mlir::Value *init, mlir::Value *stride, const LoopFrame &frame, type::Type *type) -> mlir::Value *;
  auto EmitPointerIV(mlir::Value *base, const std::vector<mlir::Value *> &init_indices, int element_step,
                     const LoopFrame &frame, type::Type *type) -> mlir::Value *;

  // Materialize a SCEV as a value inserted before the terminator of `at`; returns nullptr if the
  // expression cannot be expanded there. An enclosing loop's induction variable is materialized as
  // that loop's existing header phi (which dominates `at` when the loop contains it).
  auto Expand(const analysis::SCEV *scev, mlir::Block *at) -> mlir::Value *;
  auto MakeConstInt(int value, mlir::Block *at) -> mlir::Value *;
  template <typename BinaryOp>
  auto MakeBinary(mlir::Value *lhs, mlir::Value *rhs, mlir::Block *at) -> mlir::Value *;

  auto ContainsAddRec(const analysis::SCEV *scev) -> bool;
  auto AllUsesInLoop(mlir::Value *value, analysis::Loop *loop) -> bool;
  auto InsertBeforeTerminator(mlir::Operation *operation, mlir::Block *block) -> void;
  // Insert after the header's phi prefix, so the value dominates the whole loop body (and the exit
  // test) while keeping the phis contiguous at the block start.
  auto InsertAfterPhis(mlir::Operation *operation, mlir::Block *block) -> void;

  analysis::SCEVPass *scev_{nullptr};
  std::unordered_map<mlir::Operation *, mlir::Block *> op_to_block_;
  // Every loop-header induction phi in the current function, keyed by its (uniqued) recurrence, so an
  // enclosing loop's IV can be expanded back to a concrete dominating value for nested reduction.
  std::unordered_map<const analysis::SCEV *, mlir::Value *> iv_values_;
  // Per-loop induction-variable sharing (cleared in RunOnLoop): families keyed by step / base+indices
  // hold the canonical IV; members keyed by the full recurrence collapse identical uses onto it.
  std::unordered_map<const analysis::SCEV *, IVFamily> scalar_families_;
  std::unordered_map<const analysis::SCEV *, mlir::Value *> scalar_members_;
  std::unordered_map<std::string, IVFamily> pointer_families_;
  std::unordered_map<std::string, mlir::Value *> pointer_members_;
  int reduced_count_{0};      // multiplies replaced by an additive IV (benchmark stat)
  int gep_reduced_count_{0};  // getptr addresses replaced by a pointer IV (benchmark stat)
};

}  // namespace middleend::pass::transform

#endif
