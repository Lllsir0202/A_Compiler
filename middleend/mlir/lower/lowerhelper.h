/**
 * @brief This file defines the LowerHelper class,
 * which provides helper functions for lowering high-level MLIR operations
 * to base dialect.
 */
#ifndef MIDDLEEND_MLIR_LOWER_LOWERHELPER_H
#define MIDDLEEND_MLIR_LOWER_LOWERHELPER_H
#include "mlir/dialect/arithm/arithm.h"
#include "mlir/dialect/base/base.h"
#include "mlir/dialect/func/func.h"
#include "mlir/dialect/scf/scf.h"
#include "mlir/dialect/var/var.h"
#include "mlir/utils/operation/operation.h"

namespace middleend::mlir::lower {
class LowerHelper {
 public:
  LowerHelper() = default;
  ~LowerHelper() = default;

  /**
   * @brief Dispatch the lowering of different operations
   * since we cannot directly dispatch the lowering of different operations,
   * and via virtual functions or visitor pattern would be too heavy and coupled
   * for the dialects.
   */
  auto LowerDispatch(Operation *op) -> void;

 private:
  // Detailed lowering functions for different operations
  // TODO(loki) : implement dispatching and lowering for different operations.
};
}  // namespace middleend::mlir::lower

#endif