#ifndef MIDDLEEND_DIALECT_VAR_BASIC_H
#define MIDDLEEND_DIALECT_VAR_BASIC_H
#include "middleend/mlir/utils/operation/operation.h"

namespace middleend::mlir::var {

class VarOps : public Operation {
 public:
  ~VarOps() override = default;
  /**
   * @brief Get the name of the operation,
   * which is like the format of "dialect_name.operator_name", e.g. "arithm.add"
   */
  auto Name() const -> std::string override { return DIALECT_NAME + "." + operator_name_; }

  /**
   * @brief Get the name of the dialect, which is "arithm"
   */
  auto DialectName() const -> std::string override { return DIALECT_NAME; }
  auto Emit(std::ofstream *out) -> void override = 0;

 protected:
  explicit VarOps(std::string operator_name) : operator_name_(std::move(operator_name)) {}

 private:
  static const std::string DIALECT_NAME;
  std::string operator_name_;
};
const std::string VarOps::DIALECT_NAME = "var";

}  // namespace middleend::mlir::var

#endif