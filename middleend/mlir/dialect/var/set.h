#ifndef MIDDLEEND_MLIR_DIALECT_VAR_SET_H
#define MIDDLEEND_MLIR_DIALECT_VAR_SET_H

#include "builder/mlir/mlirbuilder.h"
#include "mlir/dialect/var/basic.h"
namespace middleend::mlir::var {

class VarSetOp : public VarOps {
 public:
  static auto Create(MLIRBuilder *builder, Value *ptr, Value *new_value) -> VarSetOp * {
    auto op = new VarSetOp();
    op->SetUp(builder, ptr, new_value);
    return op;
  }

  auto GetPtr() -> Value * { return operands_[0]; }
  auto GetNewValue() -> Value * { return operands_[1]; }
  /**
   * @note: The result of a `set` operation shall be of no use,
   * since it is just like low-level ir instruction `store`.
   */
  auto GetRes() -> Value * { return results_[0]; }
  auto Emit(std::ofstream *out) const -> void override {
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    operands_[0]->Emit(out);
    operands_[1]->Emit(out);
    *out << "\n";
  }

 private:
  VarSetOp() : VarOps("set") {}
  auto SetUp(MLIRBuilder *builder, Value *ptr, Value *new_value) -> void {
    operands_.reserve(2);
    results_.reserve(1);
    operands_.emplace_back(ptr);
    operands_.emplace_back(new_value);
    auto res = builder->GetMLIRContext()->CreateValue();
    results_.emplace_back(res);
  }
};

}  // namespace middleend::mlir::var

#endif