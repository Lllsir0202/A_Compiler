#include "middleend/mlir/dialect/scf/break.h"
#include "context/compiler_context.h"

namespace middleend::mlir::scf {
auto BreakOp::Verify(context::CompilerContext *context) -> bool {
  for (const auto &operand : operands_) {
    if (operand->GetType() == nullptr) {
      context->Print("BreakOp: operand has null type\n");
      return false;
    }
  }
  return true;
}

}  // namespace middleend::mlir::scf