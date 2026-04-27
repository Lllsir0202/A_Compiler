#ifndef MIDDLEEND_MLIR_DIALECT_ARITHM_BAISC_H
#define MIDDLEEND_MLIR_DIALECT_ARITHM_BAISC_H
#include "middleend/mlir/utils/operation/operation.h"
namespace middleend::mlir::arithm {
/**
 * @brief ArithmOps is used as a base class for all operations
 * which are included in the arithm dialect.
 * It is never used as a concrete operation.
 */
class ArithmOps : public Operation {
 public:
  ~ArithmOps() override = default;
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

  /**
   * @brief identify whether this operation is a commutative operation.
   */
  auto IsCommutative() const -> bool { return operator_name_ == "add" || operator_name_ == "mul"; }

 protected:
  explicit ArithmOps(std::string operator_name) : operator_name_(std::move(operator_name)) {}

 private:
  static const std::string DIALECT_NAME;
  std::string operator_name_;
};

const std::string ArithmOps::DIALECT_NAME = "arithm";

}  // namespace middleend::mlir::arithm
#endif