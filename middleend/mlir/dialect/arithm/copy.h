#ifndef MIDDLEEND_MLIR_DIALECT_ARITHM_COPY_H
#define MIDDLEEND_MLIR_DIALECT_ARITHM_COPY_H

#include "context/mlir_context.h"
#include "middleend/mlir/dialect/arithm/basic.h"
#include "mlir/utils/value/value.h"
namespace middleend::mlir::arithm {
/**
 * @brief Copy operation is used to update the value of a variable,
 * current design is transfering this op to an addi in LLVMIR
 */
class CopyOp : public ArithmOps {
 public:
  static auto Create(context::MLIRContext *context, mlir::Value *value) {
    auto op = new CopyOp();
    op->SetUp(context, value);
    return op;
  }

  // Used in lowering
  static auto Create(CopyOp *other) -> CopyOp * {
    auto op = new CopyOp();
    op->SetUp(other->results_[0], other->operands_[0]);
    other->operands_[0]->ReplaceAllOpUsesWith(other, op);
    return op;
  }

  /**
   * Since we might need to use and change the value and result of the operation
   * we should not make them const
   */
  auto GetValue() -> mlir::Value * { return operands_[0]; }
  auto GetRes() -> mlir::Value * { return results_[0]; }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    operands_[0]->Emit(out);
    *out << "\n";
  }

  // Verify the operation
  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  CopyOp() : ArithmOps("copy") {}
  auto SetUp(context::MLIRContext *context, mlir::Value *value) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(value);
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  };

  // Used in the lowering when we need to create a new copy operation with specific result and operand
  auto SetUp(mlir::Value *res, mlir::Value *value) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(value);
    results_.emplace_back(res);
    res->op_ = this;
  };
};
}  // namespace middleend::mlir::arithm

#endif