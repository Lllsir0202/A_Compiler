#ifndef MIDDLEEND_MLIR_DIALECT_SCF_BASIC_H
#define MIDDLEEND_MLIR_DIALECT_SCF_BASIC_H
#include "mlir/utils/operation/operation.h"
namespace middleend::mlir::scf {
/**
 * @brief The SCF dialect is used to represent the control flow operations
 */
class SCFOps : public Operation {
 public:
  ~SCFOps() override = default;

  /**
   * @brief Get the name of the operation,
   * which is like the format of "dialect_name.operator_name", e.g. "arithm.add"
   */
  auto Name() const -> std::string override { return DIALECT_NAME + "." + operator_name_; }

  /**
   * @brief Get the name of the dialect, which is "arithm"
   */
  auto DialectName() const -> std::string override { return DIALECT_NAME; }
  auto Emit(std::ofstream *out) const -> void override;

 protected:
  explicit SCFOps(std::string operator_name) : operator_name_(std::move(operator_name)) {}

 private:
  static const std::string DIALECT_NAME;
  std::string operator_name_;
};
const std::string SCFOps::DIALECT_NAME = "scf";

}  // namespace middleend::mlir::scf
#endif