#include "middleend/mlir/utils/adapter/replace/replace.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/value/value.h"

namespace middleend::mlir {
auto ReplaceAdapter::Adapt(Operation *op) -> void { ReplaceInOperation(op); }

auto ReplaceAdapter::ReplaceInOperation(Operation *op) -> void {
  // Make sure the operation is not null
  if (!Verify()) {
    throw std::runtime_error("ReplaceAdapter is not properly set up, old_value or new_value is nullptr");
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
  if (auto condition_op = DynamicOrNull<mlir::scf::ConditionOp>(op)) {
    // If the operation is a condition operation, we also need to replace the original values
    auto original_values = condition_op->GetOriginalValues();
    for (size_t i = 0; i < original_values.size(); ++i) {
      if (original_values[i] == old_value_) {
        printf("Replace original value in condition operation: %s%zu with %s%zu\n", old_value_->name_.c_str(),
               old_value_->num_, new_value_->name_.c_str(), new_value_->num_);
        condition_op->UpdateOriginalValue(i, new_value_);
        changed = true;
      }
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