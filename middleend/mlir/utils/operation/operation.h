#ifndef MIDDLEEND_MLIR_UTILS_OPERATION_OPERATION_H
#define MIDDLEEND_MLIR_UTILS_OPERATION_OPERATION_H
#include <string>
#include <vector>

namespace middleend::mlir {
class Value;
class Region;
/**
 * @brief Represents an operation in the MLIR
 * this class is the concrete base class for all operations in the MLIR
 * it will provide common interface for all sorts of operations
 * @member:
 * - operands: the operands of this operation, which are values
 * - result: the result of the operation, which is a value
 * and for SYSY language, we only have one result for each operation
 * -
 */
class Operation {
 public:
  // Operands of the operation
  std::vector<Value *> operands_;  // the operands of this operation
  // Result of the operation, can be a nullptr if the operation has no result
  // especially for statements
  Value *result_;  // the result of the operation, which is a value

  // Region of the operation
  [[maybe_unused]] std::vector<Region *> regions_;  // the regions of this operation, which are blocks

  // Print the name of a operation
  virtual auto Name() const -> std::string = 0;
};

}  // namespace middleend::mlir

#endif