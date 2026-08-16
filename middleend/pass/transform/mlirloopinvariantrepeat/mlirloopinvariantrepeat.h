#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRLOOPINVARIANTREPEAT_MLIRLOOPINVARIANTREPEAT_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRLOOPINVARIANTREPEAT_MLIRLOOPINVARIANTREPEAT_H

#include <cstddef>
#include <stdexcept>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncDefOp;
}

namespace middleend::mlir::scf {
class WhileOp;
}

namespace middleend::pass::transform {

/**
 * @brief Collapse repeated, read-only accumulator kernels to one execution.
 *
 * If an outer counted loop has no memory writes/calls, does not otherwise use
 * its IV, and maps one live integer accumulator from `acc` to `acc + delta`
 * where delta is independent of the outer IV and accumulator, its nested body
 * is executed at most once and the result is scaled by the original trip
 * count. This preserves i32 modular arithmetic while avoiding repeated
 * invariant reductions.
 */
class MLIRLoopInvariantRepeatPass : public TransformPass {
 public:
  MLIRLoopInvariantRepeatPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRLoopInvariantRepeatPass only supports MLIR stage");
    }
  }
  ~MLIRLoopInvariantRepeatPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRLoopInvariantRepeatPass"; }

 private:
  struct Candidate {
    size_t induction_index{0};
    size_t accumulator_index{0};
    mlir::Value *bound{nullptr};
    mlir::Value *accumulator_init{nullptr};
    mlir::Operation *condition_compare{nullptr};
  };

  auto RunOnFunction(mlir::func::FuncDefOp *function) -> bool;
  auto FoldInBlock(mlir::func::FuncDefOp *function, mlir::Block *block) -> bool;
  auto FindCandidate(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool;
  auto Fold(mlir::func::FuncDefOp *function, mlir::Block *parent, size_t loop_index, mlir::scf::WhileOp *loop,
            const Candidate &candidate) -> void;

  size_t loops_seen_{0};
  size_t candidates_{0};
  size_t folded_{0};
};

}  // namespace middleend::pass::transform

#endif
