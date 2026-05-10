#include "middleend/mlir/dialect/base/copy.h"
#include "context/compiler_context.h"

namespace middleend::mlir::base {
auto CopyOp::Verify(context::CompilerContext *context) -> bool {
  // CopyOp should have one operand and one result
  if (operands_.size() != 1) {
    context->Print("CopyOp: operand size is not 1\n");
    return false;
  }
  if (results_.size() != 1) {
    context->Print("CopyOp: result size is not 1\n");
    return false;
  }
  // The type of the result should be the same as the type of the operand
  if (results_[0]->GetType() != operands_[0]->GetType()) {
    context->Print("CopyOp: result type is not the same as operand type\n");
    return false;
  }
  return true;
}

auto CopyOp::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  results_[0]->Emit(out);
  *out << " = " << Name() << " ";
  operands_[0]->Emit(out);
  *out << "\n";
}

auto CopyOp::SetUp(mlir::Value *res, mlir::Value *input) -> void {
  /* CopyOp has one operand and one result */
  operands_.reserve(1);
  results_.reserve(1);
  operands_.emplace_back(input);
  results_.emplace_back(res);
  res->SetType(input->GetType());
  res->op_ = this;
}

}  // namespace middleend::mlir::base