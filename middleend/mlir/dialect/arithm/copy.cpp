#include "middleend/mlir/dialect/arithm/copy.h"
#include "context/compiler_context.h"

namespace middleend::mlir::arithm {
auto CopyOp::Verify([[maybe_unused]] context::CompilerContext *context) -> bool {
  // Check the number of operands and results
  if (operands_.size() != 1) {
    context->Print("CopyOp:operand size should be 1\n");
    return false;
  }
  if (results_.size() != 1) {
    context->Print("CopyOp:results size should be 1\n");
    return false;
  }
  // Check the type of the operand and result, they should be the same
  if (operands_[0]->GetType() != results_[0]->GetType()) {
    context->Print("CopyOp: operand and result types do not match, result got " + results_[0]->GetType()->ToString() +
                   " but operand got " + operands_[0]->GetType()->ToString() + "\n");
    return false;
  }
  return true;
}

}  // namespace middleend::mlir::arithm