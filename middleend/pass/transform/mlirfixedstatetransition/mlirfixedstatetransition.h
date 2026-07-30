#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRFIXEDSTATETRANSITION_MLIRFIXEDSTATETRANSITION_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRFIXEDSTATETRANSITION_MLIRFIXEDSTATETRANSITION_H

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "middleend/pass/transform/transform.h"

namespace middleend::pass::analysis {
class MLIRMemoryEffectAnalysis;
}

namespace middleend::mlir {
class Block;
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
 * Stop a deterministic counted transition once its complete cross-iteration
 * state reaches a fixed point.
 *
 * The pass targets loops whose counter is otherwise dead, whose calls have
 * closed-world scalar/global effects, and whose read/write globals are either
 * reset at the beginning of every iteration or are scalar state.  After one
 * iteration it compares the latter state with its entry value; equality means
 * all remaining iterations would repeat the same transition and final stores.
 */
class MLIRFixedStateTransitionPass : public TransformPass {
 public:
  MLIRFixedStateTransitionPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRFixedStateTransitionPass only supports MLIR stage");
    }
    AddDependency("MLIRMemoryEffectAnalysis");
  }
  ~MLIRFixedStateTransitionPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRFixedStateTransitionPass"; }

 private:
  struct Candidate {
    size_t induction_index{0};
    mlir::Value *bound{nullptr};
    std::vector<mlir::Value *> state_roots;
  };

  auto RunOnFunction(mlir::func::FuncDefOp *function) -> bool;
  auto TransformInBlock(mlir::func::FuncDefOp *function, mlir::Block *block) -> bool;
  auto FindCandidate(mlir::scf::WhileOp *loop, Candidate *candidate) -> bool;
  auto Transform(mlir::func::FuncDefOp *function, mlir::scf::WhileOp *loop, const Candidate &candidate) -> void;

  analysis::MLIRMemoryEffectAnalysis *memory_effects_{nullptr};
  size_t loops_seen_{0};
  size_t candidates_{0};
  size_t transformed_{0};
  size_t effect_rejects_{0};
  size_t state_rejects_{0};
  size_t dependence_rejects_{0};
};

}  // namespace middleend::pass::transform

#endif  // MIDDLEEND_PASS_TRANSFORM_MLIRFIXEDSTATETRANSITION_MLIRFIXEDSTATETRANSITION_H
