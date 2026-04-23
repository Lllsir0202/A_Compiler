#ifndef MIDDLEEND_MLIR_DIALECT_VAR_GET_H
#define MIDDLEEND_MLIR_DIALECT_VAR_GET_H
#include "middleend/builder/mlir/mlirbuilder.h"
#include "middleend/mlir/dialect/var/basic.h"
namespace middleend::mlir::var {
/**
 * @brief This operation is used to get the value of a variable defined before,
 * and it represents a high-level load instruction
 */
class VarGetOp : public VarOps {
 public:
  static auto Create(MLIRBuilder *builder, std::vector<Value *> &indices) -> VarGetOp * {
    auto op = new VarGetOp();
    op->SetUp(builder, indices);
    return op;
  }

  auto GetIndices() -> std::vector<Value *> & { return operands_; }
  auto GetRes() -> Value * { return results_[0]; }
  auto Emit(std::ofstream *out) const -> void override {
    /**
     * @example: %res = var.get %index1, %index2, ...
     */
    results_[0]->Emit(out);
    *out << " = " << Name() << " ";
    for (const auto &index : operands_) {
      index->Emit(out);
      if (index != operands_.back()) {
        *out << ", ";
      }
    }
    *out << "\n";
  }

 private:
  VarGetOp() : VarOps("get"){};
  auto SetUp(MLIRBuilder *builder, std::vector<Value *> &indices) -> void {
    operands_.reserve(indices.size());
    results_.reserve(1);
    operands_ = std::move(indices);
    auto res = builder->GetMLIRContext()->CreateValue();
    results_.emplace_back(res);
  }
};

}  // namespace middleend::mlir::var
#endif