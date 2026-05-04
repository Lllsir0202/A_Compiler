#include "middleend/mlir/dialect/base/funcdef.h"
#include "context/compiler_context.h"
namespace middleend::mlir::base {
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
  // The block in the region cannot be determined

  if (results_.size() != 1) {
    context->Print("FuncDefOp: result size is not 1\n");
    return false;
  }
  if (results_[0]->GetType() == nullptr) {
    context->Print("FuncDefOp: result type is nullptr\n");
    return false;
  }

  for (const auto &block : regions_[0]->blocks_) {
    for (const auto &op : block->operations_) {
      if (!op->Verify(context)) {
        // If any operation in the function body is invalid, the function definition is invalid
        context->Print("FuncDefOp: operation in function body is invalid\n");
        context->Print("FuncDefOp: failed to verify operation " + op->Name() + "\n");
        return false;
      }
    }
  }

  return true;
}
}  // namespace middleend::mlir::base