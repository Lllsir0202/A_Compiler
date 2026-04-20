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
 * - results: the results of the operation, which are values
 */
class Operation {
 public:
  // Print the name of a operation
  virtual auto Name() const -> std::string = 0;
  // Get the dialect name of the operation
  virtual auto DialectName() const -> std::string = 0;

  // Operands of the operation
  std::vector<Value *> operands_;  // the operands of this operation
  // Result of the operation, can be a nullptr if the operation has no result
  // especially for statements
  std::vector<Value *> results_;  // the results of the operation

  // Region of the operation
  std::vector<Region *> regions_;  // the regions of this operation, which are blocks
};

/**
 * @brief In MLIR, the module is the top-level operation
 * it is also a operation, and it will be dealt as if it is a operation.
 */
class ModuleOp : public Operation {
 public:
  auto Name() const -> std::string override { return "module"; }
  auto DialectName() const -> std::string override { return "builtin"; }

  ModuleOp();
  static auto Create() -> ModuleOp * { return new ModuleOp(); }
};

}  // namespace middleend::mlir

#endif