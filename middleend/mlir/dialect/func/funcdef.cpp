#include "middleend/mlir/dialect/func/funcdef.h"
#include "context/compiler_context.h"

namespace middleend::mlir::func {

auto FuncDefOp::Verify(context::CompilerContext *context) -> bool {
  for (const auto &arg : operands_) {
    if (arg->GetType() == nullptr) {
      context->Print("FuncDefOp: argument has no type\n");
      return false;
    }
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
}  // namespace middleend::mlir::func