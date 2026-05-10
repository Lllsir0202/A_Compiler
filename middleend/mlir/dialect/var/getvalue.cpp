#include "middleend/mlir/dialect/var/getvalue.h"
#include "context/compiler_context.h"
#include "mlir/dialect/var/def.h"
#include "type/type.h"

namespace middleend::mlir::var {
auto VarGetValOp::Verify(context::CompilerContext *context) -> bool {
  // Get value from the ptr, so there shall be exactly one operand and one result
  if (operands_.size() != 1) {
    context->Print("VarGetValOp: expected exactly one operand, but got " + std::to_string(operands_.size()) + "\n");
    return false;
  }

  if (results_.size() != 1) {
    context->Print("VarGetValOp: expected exactly one result, but got " + std::to_string(results_.size()) + "\n");
    return false;
  }

  // nullptr check for operand and result
  if (operands_[0]->GetType() == nullptr) {
    context->Print("VarGetValOp: operand has null type\n");
    return false;
  }

  if (results_[0]->GetType() == nullptr) {
    context->Print("VarGetValOp: result has null type\n");
    return false;
  }

  // We make sure all ptr is a pointer to the variable
  if (operands_[0]->GetType()->GetKind() != type::TypeKind::PTR) {
    context->Print("VarGetValOp: operand is not a pointer type\n");
    return false;
  }

  return true;
}

}  // namespace middleend::mlir::var