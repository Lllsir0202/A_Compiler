#include "middleend/mlir/verifer/verifer.h"
#include "context/compiler_context.h"
namespace middleend::mlir::verifier {
auto MLIRVerifier::Verify() -> bool {
  // For now we only verify the module operation, which is the top-level operation
  if (module_op_ == nullptr) {
    compiler_context_->Print("ModuleOp: No module operation to verify.");
    return false;
  }
  return module_op_->Verify(compiler_context_);
}

auto BaseVerifier::Verify() -> bool {
  if (module_op_ == nullptr) {
    compiler_context_->Print("Operation: No operation to verify.");
    return false;
  }

  return module_op_->Verify(compiler_context_);
}
}  // namespace middleend::mlir::verifier