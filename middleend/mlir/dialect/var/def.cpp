#include "middleend/mlir/dialect/var/def.h"
#include "context/compiler_context.h"

namespace middleend::mlir::var {
auto VarDefOp::Verify(context::CompilerContext *context) -> bool {
  for (const auto &operand : operands_) {
    // If the operand is not nullptr, we need to check its type
    if (operand != nullptr && operand->GetType() == nullptr) {
      context->Print("VarDefOp: operand has null type\n");
      return false;
    }
  }
  return true;
}

}  // namespace middleend::mlir::var