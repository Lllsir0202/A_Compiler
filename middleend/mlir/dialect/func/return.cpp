#include "middleend/mlir/dialect/func/return.h"
#include "context/compiler_context.h"

namespace middleend::mlir::func {
auto ReturnOp::Verify(context::CompilerContext *context) -> bool {
  if (operands_.empty()) {
    // void return
    if (results_[0]->GetType()->GetKind() != type::TypeKind::VOID) {
      context->Print("ReturnOp: expected void return type\n");
      return false;
    }
    return true;
  }
  // non-void return
  if (operands_[0]->GetType() != results_[0]->GetType()) {
    context->Print("ReturnOp: return type mismatch\n");
    return false;
  }
  return true;
}

}  // namespace middleend::mlir::func