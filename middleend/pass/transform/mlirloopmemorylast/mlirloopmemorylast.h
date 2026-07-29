#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRLOOPMEMORYLAST_MLIRLOOPMEMORYLAST_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRLOOPMEMORYLAST_MLIRLOOPMEMORYLAST_H

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
 * @brief Keep only the last execution of a repeated memory kernel.
 *
 * The first supported proof recognizes a constant repeated outer loop whose
 * body starts by overwriting a complete two-dimensional runtime rectangle
 * [0, bound) x [0, bound) of one object. Every later access to that object must
 * use induction variables proven inside the same rectangle, all stores must
 * target that object, and the repetition IV/results must be unobservable.
 */
class MLIRLoopMemoryLastIterationPass : public TransformPass {
 public:
  MLIRLoopMemoryLastIterationPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRLoopMemoryLastIterationPass only supports MLIR stage");
    }
  }
  ~MLIRLoopMemoryLastIterationPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRLoopMemoryLastIterationPass"; }

 private:
  struct Candidate {
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
