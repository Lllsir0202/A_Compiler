#ifndef MIDDLEEND_MLIR_DIALECT_SCF_YIELD_H
#define MIDDLEEND_MLIR_DIALECT_SCF_YIELD_H

#include <utility>
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/scf/basic.h"
#include "mlir/utils/value/value.h"

namespace middleend::mlir::scf {
class YieldOp : public SCFOps {
 public:
  ~YieldOp() override = default;

  static auto Create(context::MLIRContext *context, std::vector<mlir::Value *> operands) -> YieldOp * {
    auto op = new YieldOp();
    op->SetUp(context, operands);
    return op;
  }

  static auto Create(context::MLIRContext *context, mlir::Value *operand) -> YieldOp * {
    return Create(context, std::vector<mlir::Value *>{operand});
  }

  static auto Create(context::MLIRContext *context) -> YieldOp * {
    return Create(context, std::vector<mlir::Value *>{});
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    *out << Name();
    if (!operands_.empty()) {
      *out << " ";
      for (size_t i = 0; i < operands_.size(); ++i) {
        operands_[i]->Emit(out);
        if (i != operands_.size() - 1) {
          *out << ", ";
        }
      }
    }
    *out << "\n";
  }

  /**
   * @brief Get Operands
   */
  auto GetOperands() -> std::vector<mlir::Value *> & { return operands_; }

 private:
  YieldOp() : SCFOps("yield") {}
  /**
   * @brief yield shall have no result,
   * and the operands of the yield operation,
   * will be used to store the values to be yielded,
   * so we can just set up the operands of the yield operation,
   */
  auto SetUp([[maybe_unused]] context::MLIRContext *context, std::vector<mlir::Value *> &operands) -> void {
    operands_ = std::move(operands);
  }
};

}  // namespace middleend::mlir::scf

#endif