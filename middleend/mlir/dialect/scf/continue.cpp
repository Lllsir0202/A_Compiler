#include "middleend/mlir/dialect/scf/continue.h"
#include "context/compiler_context.h"

namespace middleend::mlir::scf {
auto ContinueOp::Verify(context::CompilerContext *context) -> bool {
  for (const auto &operand : operands_) {
    if (operand->GetType() == nullptr) {
      context->Print("ContinueOp: operand has null type\n");
      return false;
    }
  }
  return true;
}

}  // namespace middleend::mlir::scf