#ifndef MIDDLEEND_MLIR_DIALECT_BASE_BAISC_H
#define MIDDLEEND_MLIR_DIALECT_BASE_BAISC_H
#include "middleend/mlir/utils/operation/operation.h"
namespace middleend::mlir::base {
/**
 * @brief BasicOp is used as a base class for all operations
 * which are included in the base dialect.
 * It is never used as a concrete operation.
 */
class BaseOps : public Operation {
 public:
  ~BaseOps() override = default;
  /**
   * @brief Get the name of the operation,
   * which is like the format of "dialect_name.operator_name", e.g. "base.add"
   */
  auto Name() const -> std::string override { return DIALECT_NAME + "." + operator_name_; }

  /**
   * @brief Get the name of the dialect, which is "base"
   */
  auto DialectName() const -> std::string override { return DIALECT_NAME; }
  auto Emit([[maybe_unused]] std::ofstream *out) -> void override = 0;

 protected:
  explicit BaseOps(std::string operator_name) : operator_name_(std::move(operator_name)) {}

 private:
  static const std::string DIALECT_NAME;
  std::string operator_name_;
};

const std::string BaseOps::DIALECT_NAME = "base";

}  // namespace middleend::mlir::base

#endif