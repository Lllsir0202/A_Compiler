#ifndef MIDDLEEND_PASS_TRANSFORM_MLIRRETURNCANONICALIZE_MLIRRETURNCANONICALIZE_H
#define MIDDLEEND_PASS_TRANSFORM_MLIRRETURNCANONICALIZE_MLIRRETURNCANONICALIZE_H

#include <stdexcept>
#include "middleend/pass/transform/transform.h"

namespace middleend::mlir {
class Block;
class Operation;
class Region;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::func {
class FuncDefOp;
class ReturnOp;
}  // namespace middleend::mlir::func

namespace middleend::mlir::scf {
class IfOp;
}  // namespace middleend::mlir::scf

namespace middleend::pass::transform {
class MLIRReturnCanonicalizePass : public TransformPass {
 public:
  MLIRReturnCanonicalizePass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::MLIR) {
      throw std::invalid_argument("MLIRReturnCanonicalizePass only supports MLIR stage");
    }
  }
  ~MLIRReturnCanonicalizePass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MLIRReturnCanonicalizePass"; }

 private:
  auto RunOnFunction(mlir::func::FuncDefOp *func_op) -> bool;
  auto RewriteEarlyReturnIf(mlir::func::FuncDefOp *func_op, mlir::Block *block, size_t if_index, mlir::scf::IfOp *if_op)
      -> bool;
  auto IsSimpleEarlyReturnThen(mlir::scf::IfOp *if_op, mlir::func::ReturnOp **return_op) -> bool;
  auto IsBothBranchReturn(mlir::scf::IfOp *if_op, mlir::func::ReturnOp **then_return,
                          mlir::func::ReturnOp **else_return) -> bool;
  auto RebuildUses(mlir::func::FuncDefOp *func_op) -> void;
  auto ClearUsesInRegion(mlir::Region *region) -> void;
  auto RebuildUsesInOperation(mlir::Operation *operation) -> void;
};

}  // namespace middleend::pass::transform

#endif
