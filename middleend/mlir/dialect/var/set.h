#ifndef MIDDLEEND_MLIR_DIALECT_VAR_SET_H
#define MIDDLEEND_MLIR_DIALECT_VAR_SET_H

#include "context/mlir_context.h"
#include "mlir/dialect/var/basic.h"
#include "mlir/utils/value/value.h"
namespace middleend::mlir::var {

class VarSetOp : public VarOps {
 public:
  static auto Create(context::MLIRContext *context, mlir::Value *ptr, mlir::Value *new_value) -> VarSetOp * {
    auto op = new VarSetOp();
    op->SetUp(context, ptr, new_value);
    return op;
  }

  // Used in lowering
  static auto Create(VarSetOp *other) -> VarSetOp * {
    auto op = new VarSetOp();
    op->SetUp(other->GetRes(), other->GetPtr(), other->GetNewValue());
    other->GetPtr()->ReplaceAllOpUsesWith(other, op);
    other->GetNewValue()->ReplaceAllOpUsesWith(other, op);
    return op;
  }

  /**
   * @brief The set operation will just store the value to the pointer
   */
  auto GetPtr() -> mlir::Value * { return operands_[0]; }
  auto GetNewValue() -> mlir::Value * { return operands_[1]; }
  /**
   * @note: The result of a `set` operation shall be of no use,
   * since it is just like low-level ir instruction `store`.
   */
  auto GetRes() -> mlir::Value * { return results_[0]; }
  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    operands_[0]->Emit(out);
    *out << ", ";
    operands_[1]->Emit(out);
    *out << "\n";
  }

  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  VarSetOp() : VarOps("set") {}
  auto SetUp(context::MLIRContext *context, mlir::Value *ptr, mlir::Value *new_value) -> void {
    operands_.reserve(2);
    results_.reserve(1);
    operands_.emplace_back(ptr);
    operands_.emplace_back(new_value);
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  }

  auto SetUp(mlir::Value *res, mlir::Value *ptr, mlir::Value *new_value) -> void {
    operands_.reserve(2);
    results_.reserve(1);
    operands_.emplace_back(ptr);
    operands_.emplace_back(new_value);
    results_.emplace_back(res);
    res->op_ = this;
  }
};

}  // namespace middleend::mlir::var

#endif