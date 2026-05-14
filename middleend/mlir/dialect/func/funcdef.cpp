#include "middleend/mlir/dialect/func/funcdef.h"
#include "context/compiler_context.h"
#include "mlir/utils/adapter/adapter.h"

namespace middleend::mlir::func {

auto FuncDefOp::Verify(context::CompilerContext *context) -> bool {
  for (const auto &arg : operands_) {
    if (arg->GetType() == nullptr) {
      context->Print("FuncDefOp: argument has no type\n");
      return false;
    }
  }
  // the region should always be 1
  if (regions_.size() != 1) {
    context->Print("FuncDefOp: expected exactly one region, but got " + std::to_string(regions_.size()) + "\n");
    return false;
  }
  // the block in the region should always be 1
  if (regions_[0]->blocks_.size() != 1) {
    context->Print("FuncDefOp: expected exactly one block in the region, but got " +
                   std::to_string(regions_[0]->blocks_.size()) + "\n");
    return false;
  }

  const auto &ops = regions_[0]->blocks_[0]->operations_;
  // Verify all operations in the function body
  for (const auto &op : ops) {
    if (!op->Verify(context)) {
      // If any operation in the function body is invalid, the function definition is invalid
      context->Print("FuncDefOp: operation in function body is invalid\n");
      context->Print("FuncDefOp: failed to verify operation " + op->Name() + "\n");
      return false;
    }
  }
  return true;
}

auto FuncDefOp::Walk(Adapter *adapter) -> void {
  // Walk the operations in the function body
  for (const auto &region : regions_) {
    for (const auto &block : region->blocks_) {
      for (const auto &op : block->operations_) {
        op->Walk(adapter);
      }
    }
  }
}

}  // namespace middleend::mlir::func