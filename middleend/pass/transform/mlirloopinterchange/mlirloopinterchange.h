#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRLOOPINTERCHANGE_MLIRLOOPINTERCHANGE_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRLOOPINTERCHANGE_MLIRLOOPINTERCHANGE_H

#include <cstddef>
#include <stdexcept>
#include "middleend/mlir/utils/clone/operationcloner.h"
#include "middleend/pass/transform/transform.h"

namespace middleend::pass::analysis {
class MLIRMemoryEffectAnalysis;
}

namespace middleend::mlir {
class Block;
class Operation;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncDefOp;
}

namespace middleend::mlir::scf {
class WhileOp;
}

namespace middleend::pass::transform {
namespace analysis = middleend::pass::analysis;

/**
 * @brief Interchange adjacent structured loops to improve row-major locality.
 *
 * The pass follows the same high-level contract as LLVM/MLIR loop interchange:
 * operate only on a perfect rectangular nest, prove dependence legality before
 * rewriting, and require a concrete locality win.  The custom SysY MLIR uses
 * `scf.while` with loop-carried values, so the rewrite rebuilds the two loop
 * shells instead of mutating a source-level loop variable order.
 */
class MLIRLoopInterchangePass : public TransformPass {
 public:
  MLIRLoopInterchangePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage), cloner_(compiler_context) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRLoopInterchangePass only supports MLIR stage");
    }
    AddDependency("MLIRMemoryEffectAnalysis");
  }
  ~MLIRLoopInterchangePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRLoopInterchangePass"; }

 private:
  auto RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool;
  auto InterchangeInBlock(mlir::Block *block) -> bool;
  auto TryInterchange(mlir::Block *parent, size_t index) -> bool;

  mlir::utils::OperationCloner cloner_;
  analysis::MLIRMemoryEffectAnalysis *memory_effects_{nullptr};

  size_t candidates_{0};
  size_t interchanged_{0};
  size_t shape_rejects_{0};
  size_t domain_rejects_{0};
  size_t dependence_rejects_{0};
  size_t profitability_rejects_{0};
};

}  // namespace middleend::pass::transform

#endif
