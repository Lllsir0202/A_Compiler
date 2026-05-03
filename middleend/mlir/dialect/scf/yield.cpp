#include "middleend/mlir/dialect/scf/yield.h"
#include "context/compiler_context.h"

namespace middleend::mlir::scf {
auto YieldOp::Verify(context::CompilerContext *context) -> bool {
  for (const auto &operand : operands_) {
    if (operand->GetType() == nullptr) {
      context->Print("YieldOp: operand has null type\n");
      return false;
    }
  }
  return true;
}

}  // namespace middleend::mlir::scf