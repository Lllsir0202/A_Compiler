#include "middleend/mlir/utils/value/value.h"
#include <stdexcept>
#include "compiler_context.h"
#include "middleend/mlir/utils/operation/operation.h"

namespace middleend::mlir {
auto Value::AddUse(Operation *op, std::size_t index) -> void {
  if (op == nullptr) {
    throw std::runtime_error("The operation that uses this value is nullptr");
  }
  if (index >= op->operands_.size()) {
    printf("The operand index is out of range: %zu, but the operation has only %zu operands\n", index,
           op->operands_.size());
    throw std::runtime_error("The operand index is out of range");
  }
  uses_.emplace_back(new Use{op, index});
}

auto Value::ReplaceAllUsesWith(Value *new_value) -> void {
  for (auto use : uses_) {
    auto op = use->operation_;
    auto index = use->operand_index_;
    if (op == nullptr) {
      throw std::runtime_error("The operation that uses this value is nullptr");
    }
    if (index >= op->operands_.size()) {
      printf("The operand index is out of range: %zu, but the operation has only %zu operands\n", index,
             op->operands_.size());
      printf("op is %s\n", op->Name().c_str());
      printf("value is %s%zu\n", name_.c_str(), num_);
      printf("new value is %s%zu\n", new_value->name_.c_str(), new_value->num_);
      printf("op operands size is %zu\n", op->operands_.size());
      printf("op operands are:\n");
      printf("index\toperand name\n");
      for (size_t i = 0; i < op->operands_.size(); ++i) {
        auto operand = op->operands_[i];
        if (operand != nullptr) {
          printf("%zu\t%s%zu\n", index, operand->name_.c_str(), operand->num_);
        } else {
          printf("%zu\tnullptr\n", i);
        }
      }
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

auto Value::ReplaceAllOpUsesWith(Operation *old_op, Operation *new_op, size_t new_index) -> void {
  for (auto &use : uses_) {
    auto op = use->operation_;
    if (op == nullptr) {
      throw std::runtime_error("The operation that uses this value is nullptr");
    }
    // Replace the operation with the new operation
    if (op == old_op) {
      use->operation_ = new_op;

      use->operand_index_ = new_index;
    }
  }
}

// This function is used for those operations that have the same operands and result
auto Value::ReplaceAllOpUsesWith(Operation *old_op, Operation *new_op) -> void {
  for (auto &use : uses_) {
    auto op = use->operation_;
    if (op == nullptr) {
      throw std::runtime_error("The operation that uses this value is nullptr");
    }
    // Replace the operation with the new operation
    // Do not change index
    if (op == old_op) {
      use->operation_ = new_op;
    }
  }
}

}  // namespace middleend::mlir