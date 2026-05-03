#include "middleend/mlir/dialect/var/set.h"
#include "context/compiler_context.h"

namespace middleend::mlir::var {
auto VarSetOp::Verify(context::CompilerContext *context) -> bool {
  for (const auto &operand : operands_) {
    if (operand == nullptr) {
      context->Print("VarSetOp: operand is null\n");
      return false;
    }
  }
  if (results_[0] == nullptr) {
    context->Print("VarSetOp: result is null\n");
    return false;
  }
  if (results_[0]->GetType() == nullptr) {
    context->Print("VarSetOp: result has null type\n");
    return false;
  }
  if (results_[0]->GetType()->GetKind() != type::TypeKind::UNKNOWN) {
    context->Print("VarSetOp: result has invalid type\n");
    return false;
  }
  return true;
}

}  // namespace middleend::mlir::var