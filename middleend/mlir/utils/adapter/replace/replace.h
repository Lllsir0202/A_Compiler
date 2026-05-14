#ifndef MIDDLEEND_MLIR_UTILS_ADAPTER_REPLACE_REPLACE_H
#define MIDDLEEND_MLIR_UTILS_ADAPTER_REPLACE_REPLACE_H
#include "middleend/mlir/utils/adapter/adapter.h"
namespace middleend::mlir {
class Value;

class ReplaceAdapter : public Adapter {
 public:
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

}  // namespace middleend::mlir
#endif