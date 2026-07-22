#ifndef MIDDLEEND_MLIR_DIALECT_VAR_GETPTR_H
#define MIDDLEEND_MLIR_DIALECT_VAR_GETPTR_H

#include <cstddef>
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/var/basic.h"
namespace middleend::mlir::var {

class VarGetPtrOp : public VarOps {
 public:
  static auto Create(context::MLIRContext *context, mlir::Value *ptr, std::vector<mlir::Value *> indices = {})
      -> VarGetPtrOp * {
    auto op = new VarGetPtrOp();
    op->SetUp(context, ptr, std::move(indices));
    return op;
  }

  // Used in lowering
  static auto Create(VarGetPtrOp *other) -> VarGetPtrOp * {
    auto op = new VarGetPtrOp();
    op->SetUp(other->GetRes(), other->GetPtr(), other->GetIndices());
    other->GetPtr()->ReplaceAllOpUsesWith(other, op);
    for (auto &index : other->GetIndices()) {
      if (index != nullptr) {
        index->ReplaceAllOpUsesWith(other, op);
      }
    }
    return op;
  }

  /**
   * @brief Since the operation of getting a variable can be
   * accessing an array element, thus it will need a list of indices to
   * specify which element to access, and the result of this operation will be the res_
   */
  auto GetIndices() -> std::vector<mlir::Value *> {
    auto indices = std::vector<mlir::Value *>{};
    for (size_t operand_index = 1; operand_index < operands_.size(); operand_index++) {
      indices.emplace_back(operands_[operand_index]);
    }
    return indices;
  }
  auto GetPtr() -> mlir::Value * { return operands_[0]; }
  auto GetRes() -> mlir::Value * { return results_[0]; }
  auto Emit(std::ofstream *out) -> void override {
    /**
     * @example: %res = var.getptr %index1, %index2, ...
     */
    PrintIndent(out);
    results_[0]->Emit(out);
    *out << " = " << Name() << " ptr ";
    operands_[0]->Emit(out);
    // Means has dimensions
    if (operands_.size() > 1) {
      *out << ": ";
      for (size_t operand_index = 1; operand_index < operands_.size(); operand_index++) {
        operands_[operand_index]->Emit(out);
        if (operand_index != operands_.size() - 1) {
          *out << ", ";
        }
      }
    }
    *out << "\n";
  }

  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  VarGetPtrOp() : VarOps("getptr") {};
  auto SetUp(context::MLIRContext *context, mlir::Value *ptr, std::vector<mlir::Value *> indices = {}) -> void {
    operands_.reserve(indices.size() + 1);
    results_.reserve(1);
    operands_.emplace_back(ptr);
    operands_.insert(operands_.end(), indices.begin(), indices.end());
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  }

  // Used in lowering
  auto SetUp(mlir::Value *res, mlir::Value *ptr, std::vector<mlir::Value *> indices = {}) -> void {
    operands_.reserve(indices.size() + 1);
    results_.reserve(1);
    operands_.emplace_back(ptr);
    operands_.insert(operands_.end(), indices.begin(), indices.end());
    results_.emplace_back(res);
    res->op_ = this;
  }
};
}  // namespace middleend::mlir::var

#endif