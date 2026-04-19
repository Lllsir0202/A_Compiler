#ifndef MIDDLEEND_MLIR_UTILS_VALUE_VALUE_H
#define MIDDLEEND_MLIR_UTILS_VALUE_VALUE_H

#include <cstddef>
#include <string>
namespace middleend::mlir {
class Operation;
/**
 * @brief Represents a value in the MLIR
 * this class is the concrete base class for all values in the MLIR
 * it will provide common interface for all sorts of values
 * @member:
 * - op: the operation that produces this value
 * - [maybe_unused] num: the number for output, i.e. %1 = base.add %0, %0
 */
class Value {
 public:
  Operation *op_;                      // the operation that produces this value
  [[maybe_unused]] std::size_t num_;   // the number for output, i.e. %1 = base.add %0, %0
  [[maybe_unused]] std::string name_;  // the name of this value
};

}  // namespace middleend::mlir
#endif