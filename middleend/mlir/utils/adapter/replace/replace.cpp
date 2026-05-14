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
  bool changed = false;
  for (auto &operand : op->operands_) {
    if (operand == old_value_) {
      operand = new_value_;
      changed = true;
    }
  }
  // Update the users of the old value to use the new value
  if (changed) {
    auto use = old_value_->GetUseByOperation(op);
    if (use == nullptr) {
      return;
    }
    auto index = use->operand_index_;
    old_value_->RemoveUse(op);
    new_value_->AddUse(op, index);
  }
}

}  // namespace middleend::mlir