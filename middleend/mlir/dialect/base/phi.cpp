#include "middleend/mlir/dialect/base/phi.h"
#include "context/compiler_context.h"
#include "mlir/utils/struct/block.h"
#include "mlir/utils/value/value.h"
namespace middleend::mlir::base {

auto PhiOp::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  results_[0]->Emit(out);
  *out << " = " << Name() << " ";
  for (size_t i = 0; i < operands_.size(); i++) {
    auto value = operands_[i];
    auto block = blocks_[i];
    *out << "[";
    value->Emit(out);
    *out << ", " << block->Name() << "]";
    if (i != operands_.size() - 1) {
      *out << ", ";
    }
  }
  *out << '\n';
}

auto PhiOp::Verify(context::CompilerContext *context) -> bool {
  if (results_.size() != 1) {
    context->Print("PhiOp must have exactly one result");
    return false;
  }
  if (operands_.size() != blocks_.size()) {
    context->Print("PhiOp must have the same number of incoming values and blocks");
    return false;
  }
  auto result_type = results_[0]->GetType();
  for (size_t i = 0; i < operands_.size(); ++i) {
    auto value = operands_[i];
    auto block = blocks_[i];
    if (value == nullptr) {
      context->Print("PhiOp incoming value cannot be null");
      return false;
    }
    if (block == nullptr) {
      context->Print("PhiOp incoming block cannot be null");
      return false;
    }
    // Check all incoming values have the same type as the result value
    if (result_type != value->GetType()) {
      context->Print("PhiOp incoming value type must match the result type");
      return false;
    }
  }
  return true;
}
}  // namespace middleend::mlir::base