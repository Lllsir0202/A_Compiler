#include "middleend/mlir/utils/adapter/replace/replace.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/value/value.h"

namespace middleend::mlir {
auto ReplaceAdapter::Adapt(Operation *op) -> void { ReplaceInOperation(op); }

auto ReplaceAdapter::ReplaceInOperation(Operation *op) -> void {
  // Make sure the operation is not null
  if (!Verify()) {
    return;
  }
  // We only replace the operands of the operation,
  // and do not consider the case which the operation has a block or something else
  for (auto &operand : op->operands_) {
    if (operand == old_value_) {
      operand = new_value_;
    }
  }
  // Update the users of the old value to use the new value
  for (auto *user : old_value_->GetUses()) {
    for (auto &operand : user->operation_->operands_) {
      if (operand == old_value_) {
        operand = new_value_;
      }
    }
  }
}

}  // namespace middleend::mlir