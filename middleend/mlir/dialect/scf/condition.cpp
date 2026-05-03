#include "middleend/mlir/dialect/scf/condition.h"
#include "context/compiler_context.h"

namespace middleend::mlir::scf {

auto ConditionOp::Verify([[maybe_unused]] context::CompilerContext *context) -> bool {
  for (const auto &operand : operands_) {
    if (operand->GetType() == nullptr) {
      context->Print("ConditionOp: operand has null type\n");
      return false;
    }
  }
  return true;
}
}  // namespace middleend::mlir::scf