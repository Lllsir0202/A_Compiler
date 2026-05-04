#include "middleend/mlir/dialect/scf/inits.h"
#include "context/compiler_context.h"

namespace middleend::mlir::scf {
auto InitsOp::SetUp(context::MLIRContext *context, std::vector<Value *> &operands) -> void {
  operands_.reserve(operands.size());
  results_.reserve(operands.size());
  operands_ = std::move(operands);
  for (size_t i = 0; i < operands_.size(); ++i) {
    auto operand = operands_[i];
    auto res = context->CreateValue();
    res->op_ = this;
    res->SetType(operand->GetType());
    context->GetResourceContext()->SetArgValueName(res, i);
    results_.emplace_back(res);
  }
}

auto InitsOp::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  for (size_t i = 0; i < results_.size(); ++i) {
    results_[i]->Emit(out);
    if (i != results_.size() - 1) {
      *out << ", ";
    }
  }
  *out << " = " << Name() << " ";
  for (size_t i = 0; i < operands_.size(); ++i) {
    operands_[i]->Emit(out);
    if (i != operands_.size() - 1) {
      *out << ", ";
    }
  }
  *out << "\n";
}

auto InitsOp::Verify(context::CompilerContext *context) -> bool {
  // Inits operation should have at least one operand and the same number of results as operands
  if (operands_.empty()) {
    context->Print("InitsOp: operand size is 0\n");
    return false;
  }
  if (results_.size() != operands_.size()) {
    context->Print("InitsOp: result size is not equal to operand size\n");
    return false;
  }
  for (size_t i = 0; i < operands_.size(); ++i) {
    if (results_[i]->GetType() != operands_[i]->GetType()) {
      context->Print("InitsOp: result type is not equal to operand type\n");
      return false;
    }
  }
  return true;
}

}  // namespace middleend::mlir::scf