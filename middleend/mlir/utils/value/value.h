#ifndef MIDDLEEND_MLIR_UTILS_VALUE_VALUE_H
#define MIDDLEEND_MLIR_UTILS_VALUE_VALUE_H

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>
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
  Operation *op_;                      // the operation that produces this value
  [[maybe_unused]] std::size_t num_;   // the number for output, i.e. %1 = base.add %0, %0
  [[maybe_unused]] std::string name_;  // the name of this value
  std::vector<Use *> uses_;            // all operations that use this value as operand
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
  auto ReplaceAllUsesWith(Value *new_value) -> void;
  static auto Create() -> Value * { return new Value(); }

  auto Emit(std::ofstream *out) const -> void { *out << name_ << num_; }
};

}  // namespace middleend::mlir
#endif