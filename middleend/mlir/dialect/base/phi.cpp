#include "middleend/mlir/dialect/base/phi.h"
#include "context/compiler_context.h"
#include "mlir/utils/struct/block.h"
#include "mlir/utils/value/value.h"
namespace middleend::mlir::base {

auto PhiOp::Emit(std::ofstream *out) -> void {
  PrintIndent(out);
  results_[0]->Emit(out);
  *out << " = " << Name() << " ";
  for (size_t i = 0; i < incoming_.size(); i++) {
    auto &[value, block] = incoming_[i];
    *out << "[";
    value->Emit(out);
    *out << ", " << block->Name() << "]";
    if (i != incoming_.size() - 1) {
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
  for (const auto &[value, block] : incoming_) {
    if (value == nullptr) {
      context->Print("PhiOp incoming value cannot be null");
      return false;
    }
    if (block == nullptr) {
      context->Print("PhiOp incoming block cannot be null");
      return false;
    }
  }
  return true;
}
}  // namespace middleend::mlir::base