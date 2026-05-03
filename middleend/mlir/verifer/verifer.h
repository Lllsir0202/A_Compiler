#ifndef MIDDLEEND_MLIR_VERIFIER_VERIFIER_H
#define MIDDLEEND_MLIR_VERIFIER_VERIFIER_H

#include "mlir/utils/operation/operation.h"
namespace middleend::mlir::verifier {
class MLIRVerifier {
 public:
  MLIRVerifier(mlir::ModuleOp *module_op, context::CompilerContext *compiler_context)
      : module_op_(module_op), compiler_context_(compiler_context) {}
  auto Verify() -> bool;

 private:
  mlir::ModuleOp *module_op_;
  context::CompilerContext *compiler_context_;
};

}  // namespace middleend::mlir::verifier

#endif