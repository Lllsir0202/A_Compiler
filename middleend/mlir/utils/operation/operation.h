#ifndef MIDDLEEND_MLIR_UTILS_OPERATION_OPERATION_H
#define MIDDLEEND_MLIR_UTILS_OPERATION_OPERATION_H
#include <fstream>
#include <string>
#include <vector>
#include "mlir/utils/struct/block.h"

namespace context {
class CompilerContext;
class MLIRContext;
}  // namespace context
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
  virtual ~Operation();
  virtual auto Emit(std::ofstream *out) -> void = 0;

  // Provide interface to verify the operation, which will be used to check
  virtual auto Verify(context::CompilerContext *context) -> bool = 0;

  // Operands of the operation
  std::vector<Value *> operands_;  // the operands of this operation
  // Result of the operation, can be a nullptr if the operation has no result
  // especially for statements
  std::vector<Value *> results_;  // the results of the operation, NORMALLY only one

  // Region of the operation
  std::vector<Region *> regions_;  // the regions of this operation, which are blocks

  auto PrintIndent(std::ofstream *out) const -> void;
  auto IncreaseIndent() -> void { indent_level_++; }
  auto DecreaseIndent() -> void { indent_level_--; }
  auto SetIndentLevel(int level) -> void { indent_level_ = level; }
  int indent_level_;  // the indent level of this operation, which is used for printing
};

/**
 * @brief In MLIR, the module is the top-level operation
 * it is also a operation, and it will be dealt as if it is a operation.
 */
class ModuleOp : public Operation {
 public:
  auto Name() const -> std::string override { return "module"; }
  auto DialectName() const -> std::string override { return "builtin"; }
  auto Emit(std::ofstream *out) -> void override;
  auto Verify(context::CompilerContext *context) -> bool override;
  static auto Create() -> ModuleOp * {
    auto moduleop = new ModuleOp();
    /**
     * @brief initialize the module operation
     */
    moduleop->SetUp();
    return moduleop;
  }

  /**
   * @brief the module operation will always have only one region and one block
   */
  auto GetBlock() -> Block *;

  auto GetRegion() -> Region *;

 private:
  ModuleOp() = default;
  auto SetUp() -> void;
};

// Judge the operation is what kind of operation
template <typename T>
auto DynamicOrNull(Operation *op) -> T * {
  // Since there might be different operations with the same name in different dialects,
  // so we need
  return dynamic_cast<T *>(op);
}

}  // namespace middleend::mlir

#endif