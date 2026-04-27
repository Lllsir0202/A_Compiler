#ifndef MIDDLEEND_MLIR_DIALECT_VAR_GET_H
#define MIDDLEEND_MLIR_DIALECT_VAR_GET_H
#include "middleend/mlir/dialect/var/basic.h"
#include "mlir_context.h"
namespace middleend::mlir::var {
/**
 * @brief This operation is used to get the value of a variable defined before,
 * and it represents a high-level load instruction
 */
class VarGetValOp : public VarOps {
 public:
  static auto Create(context::MLIRContext *context, Value *ptr) -> VarGetValOp * {
    auto op = new VarGetValOp();
    op->SetUp(context, ptr);
    return op;
  }

  /**
   * @brief The getval operation is used AFTER getptr operation,
   * 'getptr' operation will get the pointer of the variable,
   * and 'getval' operation will get the value of the variable.
   */
  auto GetPtr() -> Value * { return operands_[0]; }
  auto GetRes() -> Value * { return results_[0]; }
  auto Emit(std::ofstream *out) -> void override {
    /**
     * @example: %res = var.get %index1, %index2, ...
     */
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    operands_[0]->Emit(out);
    *out << "\n";
  }

 private:
  VarGetValOp() : VarOps("getval"){};
  auto SetUp(context::MLIRContext *context, Value *ptr) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(ptr);
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  }
};

}  // namespace middleend::mlir::var
#endif