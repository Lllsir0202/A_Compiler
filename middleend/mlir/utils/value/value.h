#ifndef MIDDLEEND_MLIR_UTILS_VALUE_VALUE_H
#define MIDDLEEND_MLIR_UTILS_VALUE_VALUE_H

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>
#include "type/type.h"
namespace middleend::mlir {
class Operation;

/**
 * @brief Represents a use of a value in the MLIR
 * this class is used to represent the use of a value in an operation
 * it has two members:
 * - operation: the operation that uses this value
 * - operand_index: the index of the operand in the operation that uses this value
 */
class Use {
 public:
  Operation *operation_;       // the operation that uses this value
  std::size_t operand_index_;  // the index of the operand in the operation
};

/**
 * @brief Represents a value in the MLIR
 * this class is the concrete base class for all values in the MLIR
 * it will provide common interface for all sorts of values
 * @member:
 * - op: the operation that produces this value
 * - [maybe_unused] num: the number for output, i.e. %1 = base.add %0, %0
 * - [maybe_unused] name: the name of this value, which can be used for debugging and printing
 * - uses: all operations that use this value as operand
 */
class Value {
 public:
  Operation *op_;            // the operation that produces this value
  std::size_t num_;          // the number for output, i.e. %1 = base.add %0, %0
  std::string name_;         // the name of this value
  std::vector<Use *> uses_;  // all operations that use this value as operand
  type::Type *type_;         // the type of this value
  ~Value() {
    for (auto use : uses_) {
      delete use;
    }
  }

  auto AddUse(Operation *op, std::size_t index) -> void { uses_.emplace_back(new Use{op, index}); }
  [[maybe_unused]] auto RemoveUse(Operation *op) -> void {
    for (auto it = uses_.begin(); it != uses_.end(); ++it) {
      if ((*it)->operation_ == op) {
        uses_.erase(it);
        return;
      }
    }
  }

  /**
   * @brief Get all uses
   */
  auto GetUses() -> std::vector<Use *> & { return uses_; }

  /**
   * @brief Replace all uses of this value with another value
   * i.e. if we have an operation like %1 = base.add %0, and we want to replace all uses of %0 with %2,
   * then we can call %0->ReplaceAllUsesWith(%2), and then the operation will be changed to %1 = base.add %2
   * @note: this function will not change the operation that produces this value, so the op_ member of this value will
   * not be changed, but the operand of the operations that use this value will be changed to the new value.
   * @example:
   * %1 = base.add %0, %0
   * %2 = base.mul %1, %0
   * if we call %0->ReplaceAllUsesWith(%3), then the operations will be changed to:
   * %1 = base.add %3, %3
   * %2 = base.mul %1, %3
   */
  auto ReplaceAllUsesWith(Value *new_value) -> void;
  static auto Create() -> Value * { return new Value(); }

  /**
   * @brief Emit the value to the output stream
   * i.e. %x1, %y2, etc.
   */
  auto Emit(std::ofstream *out) -> void {
    *out << "%";
    if (!name_.empty()) {
      *out << name_ << "_";
    }
    *out << num_;
  }

  /**
   * @brief Set the type of this value
   */
  auto SetType(type::Type *type) -> void { type_ = type; }
  /**
   * @brief Get the type of this value
   */
  auto GetType() -> type::Type * { return type_; }

 private:
  Value() = default;
};

}  // namespace middleend::mlir
#endif