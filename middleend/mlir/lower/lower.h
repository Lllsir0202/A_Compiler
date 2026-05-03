/**
 * @brief This file defines the interface for lowering High-level IR to
 * Low-level IR, which is the last stage of the middleend before transforming
 * to LLVM IR.
 */
#ifndef MIDDLEEND_MLIR_LOWER_LOWER_H
#define MIDDLEEND_MLIR_LOWER_LOWER_H
#include "mlir/utils/operation/operation.h"
namespace context {
class CompilerContext;
}
namespace middleend::mlir::lower {
class MLIRLower {
 public:
  explicit MLIRLower(context::CompilerContext *compiler_context);
  ~MLIRLower() = default;

  auto Lower() -> void;

 private:
  context::CompilerContext *compiler_context_;
  mlir::ModuleOp *module_op_;
};
}  // namespace middleend::mlir::lower

#endif