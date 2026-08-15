#ifndef MIDDLEEND_PASS_TRANSFORM_DIVISIBILITYSIMPLIFY_DIVISIBILITYSIMPLIFY_H
#define MIDDLEEND_PASS_TRANSFORM_DIVISIBILITYSIMPLIFY_DIVISIBILITYSIMPLIFY_H

#include <cstddef>
#include <stdexcept>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
}

namespace middleend::pass::analysis {
class MLIRFunctionCFG;
}

namespace middleend::pass::transform {

/**
 * @brief Use a dominating divisibility branch to simplify signed power-of-two
 * division.
 *
 * On a CFG edge proving `x % 2^k == 0`, signed `x / 2^k` is exactly
 * `x >> k` even when x is negative.  The first scope requires the proof edge
 * to be the block's sole direct predecessor.
 */
class DivisibilitySimplifyPass : public TransformPass {
 public:
  DivisibilitySimplifyPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("DivisibilitySimplifyPass only supports BASEIR stage");
    }
    AddDependency("CFGPass");
  }
  ~DivisibilitySimplifyPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "DivisibilitySimplifyPass"; }

 private:
  auto RunOnFunction(analysis::MLIRFunctionCFG *function) -> bool;
  auto RunOnBlock(analysis::MLIRFunctionCFG *function, mlir::Block *block) -> bool;

  size_t candidates_{0};
  size_t simplified_{0};
};

}  // namespace middleend::pass::transform

#endif
