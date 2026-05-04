/**
 * @brief This file defines the interface for lowering High-level IR to
 * Low-level IR, which is the last stage of the middleend before transforming
 * to LLVM IR.
 */
#ifndef MIDDLEEND_MLIR_LOWER_LOWER_H
#define MIDDLEEND_MLIR_LOWER_LOWER_H
#include "middleend/mlir/lower/lowerhelper.h"
#include "middleend/mlir/utils/operation/operation.h"
namespace context {
class CompilerContext;
}
namespace middleend::mlir::lower {
class MLIRLower {
 public:
  explicit MLIRLower(context::CompilerContext *compiler_context);
  ~MLIRLower() = default;

  auto Lower() -> int;

 private:
  context::CompilerContext *compiler_context_;
  mlir::ModuleOp *module_op_;
  mlir::ModuleOp *new_module_op_;
  LowerHelper *lower_helper_;
};
}  // namespace middleend::mlir::lower

#endif