#include "middleend/mlir/dialect/var/getptr.h"
#include "context/compiler_context.h"

namespace middleend::mlir::var {
auto VarGetPtrOp::Verify(context::CompilerContext *context) -> bool {
  auto ptr = operands_[0];
  if (ptr->GetType() == nullptr) {
    context->Print("VarGetPtrOp: operand has null type\n");
    return false;
  }
  if (ptr->GetType() == nullptr) {
    context->Print("VarGetPtrOp: operand has null type\n");
    return false;
  }
  for (size_t i = 1; i < operands_.size(); i++) {
    auto operand = operands_[i];
    if (operand->GetType() == nullptr) {
      context->Print("VarGetPtrOp: operand has null type\n");
      return false;
    }
    // Only allow integer type as the indices of the getptr operation
    if (operand->GetType()->GetKind() != type::TypeKind::INT) {
      context->Print("VarGetPtrOp: operand has invalid type, index " + std::to_string(i) + " got " +
                     operand->GetType()->ToString() + "\n");
      return false;
    }
  }
  if (results_[0]->GetType() == nullptr) {
    context->Print("VarGetPtrOp: result has null type\n");
    return false;
  }
  if (results_[0]->GetType()->GetKind() != type::TypeKind::PTR) {
    context->Print("VarGetPtrOp: result has invalid type, got " + results_[0]->GetType()->ToString() + "\n");
    return false;
  }

  return true;
}
}  // namespace middleend::mlir::var