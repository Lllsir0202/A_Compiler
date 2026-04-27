#include "middleend/mlir/utils/value/value.h"
#include <stdexcept>
#include "middleend/mlir/utils/operation/operation.h"

namespace middleend::mlir {

auto Value::ReplaceAllUsesWith(Value *new_value) -> void {
  for (auto use : uses_) {
    auto op = use->operation_;
    auto index = use->operand_index_;
    if (op == nullptr) {
      throw std::runtime_error("The operation that uses this value is nullptr");
    }
    if (index >= op->operands_.size()) {
      throw std::runtime_error("The operand index is out of range");
    }
    // Replace the operand in the operation with the new value
    op->operands_[index] = new_value;
    // Add the use of the new value to the operation
    new_value->AddUse(op, index);
  }
  for (auto use : uses_) {
    delete use;
  }
  uses_.clear();
}

}  // namespace middleend::mlir