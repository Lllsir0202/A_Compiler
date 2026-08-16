#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRCONSTANTARRAYREDUCTION_MLIRCONSTANTARRAYREDUCTION_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRCONSTANTARRAYREDUCTION_MLIRCONSTANTARRAYREDUCTION_H

#include <cstddef>
#include <stdexcept>

#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
}

namespace middleend::mlir::func {
class FuncDefOp;
}

namespace middleend::pass::transform {

/**
 * @brief Forward a completely known local-array state through a sum reduction.
 *
 * The pass recognizes a non-escaping, fixed-size integer array whose complete
 * state is established by a counted fill and constant-index stores.  A later
 * canonical full-domain sum is replaced by `initial_accumulator + known_sum`.
 * If that exposes a positive affine modular recurrence and its closed-form
 * multiply is proven not to overflow, the repeated recurrence is replaced by
 * a period-reduced expression.
 * All accesses and loop domains must be proven exactly; partial or ambiguous
 * states are left unchanged.
 */
class MLIRConstantArrayReductionPass : public TransformPass {
 public:
  MLIRConstantArrayReductionPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRConstantArrayReductionPass only supports MLIR stage");
    }
  }
  ~MLIRConstantArrayReductionPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRConstantArrayReductionPass"; }

 private:
  auto RunOnFunction(mlir::func::FuncDefOp *function) -> bool;

  size_t arrays_seen_{0};
  size_t reductions_seen_{0};
  size_t folded_{0};
  size_t recurrences_closed_{0};
};

}  // namespace middleend::pass::transform

#endif
