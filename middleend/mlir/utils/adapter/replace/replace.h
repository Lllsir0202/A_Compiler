#ifndef MIDDLEEND_MLIR_UTILS_ADAPTER_REPLACE_REPLACE_H
#define MIDDLEEND_MLIR_UTILS_ADAPTER_REPLACE_REPLACE_H
#include "middleend/mlir/utils/adapter/adapter.h"
namespace middleend::mlir {
class Value;

class ReplaceAdapter : public Adapter {
 public:
  ~ReplaceAdapter() override = default;
  auto Adapt(Operation *op) -> void override;
  auto SetReplace(Value *old_value, Value *new_value) -> void {
    old_value_ = old_value;
    new_value_ = new_value;
  }

 private:
  // Replace a operation's value with another value
  Value *old_value_;
  Value *new_value_;

  auto ReplaceInOperation(Operation *op) -> void;
  auto Verify() -> bool { return old_value_ != nullptr && new_value_ != nullptr; }
};

class ReplaceOriginsExceptOpAdapter : public Adapter {
 public:
  ~ReplaceOriginsExceptOpAdapter() override = default;
  auto Adapt(Operation *op) -> void override;
  auto SetExcept(Operation *except_op) -> void { except_op_ = except_op; }
  auto SetReplace(Value *origin_value, Value *new_value) -> void {
    origin_value_ = origin_value;
    new_value_ = new_value;
  }

 private:
  // Replace a operation's value with another value
  Value *origin_value_;
  Value *new_value_;
  Operation *except_op_;

  auto ReplaceInOperation(Operation *op) -> void;
  auto Verify() -> bool { return origin_value_ != nullptr && new_value_ != nullptr && except_op_ != nullptr; }
};

}  // namespace middleend::mlir
#endif