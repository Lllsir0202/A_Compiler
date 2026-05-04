#ifndef MIDDLEEND_MLIR_DIALECT_VAR_GET_H
#define MIDDLEEND_MLIR_DIALECT_VAR_GET_H
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/var/basic.h"
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

  // Used in lowering
  static auto Create(VarGetValOp *other) -> VarGetValOp * {
    auto new_op = new VarGetValOp();
    new_op->SetUp(other->GetRes(), other->GetPtr());
    other->GetPtr()->ReplaceAllOpUsesWith(other, new_op);
    return new_op;
  }

  /**
   * @brief The getval operation is used AFTER getptr operation,
   * 'getptr' operation will get the pointer of the variable,
   * and 'getval' operation will get the value of the variable.
   */
  auto GetPtr() -> mlir::Value * { return operands_[0]; }
  auto GetRes() -> mlir::Value * { return results_[0]; }
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

  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  VarGetValOp() : VarOps("getval"){};
  auto SetUp(context::MLIRContext *context, mlir::Value *ptr) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(ptr);
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  }

  // Used in lowering
  auto SetUp(mlir::Value *res, mlir::Value *ptr) -> void {
    operands_.reserve(1);
    results_.reserve(1);
    operands_.emplace_back(ptr);
    results_.emplace_back(res);
    res->op_ = this;
  }
};

}  // namespace middleend::mlir::var
#endif