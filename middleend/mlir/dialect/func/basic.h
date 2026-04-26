#ifndef MIDDLEEND_MLIR_DIALECT_FUNC_BASIC_H
#define MIDDLEEND_MLIR_DIALECT_FUNC_BASIC_H

#include <utility>

#include "middleend/mlir/utils/operation/operation.h"
namespace middleend::mlir::func {
class FuncOps : public Operation {
 public:
  ~FuncOps() override = default;
  /**
   * @brief Get the name of the operation,
   * which is like the format of "dialect_name.operator_name", e.g. "func.func"
   */
  auto Name() const -> std::string override { return DIALECT_NAME + "." + operator_name_; }

  /**
   * @brief Get the name of the dialect, which is "func"
   */
  auto DialectName() const -> std::string override { return DIALECT_NAME; }
  auto Emit(std::ofstream *out) const -> void override = 0;

 protected:
  explicit FuncOps(std::string operator_name) : operator_name_(std::move(operator_name)) {}

 private:
  static const std::string DIALECT_NAME;
  std::string operator_name_;
};
const std::string FuncOps::DIALECT_NAME = "func";

}  // namespace middleend::mlir::func

#endif