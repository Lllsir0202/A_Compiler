#ifndef MIDDLEEND_MLIR_DIALECT_VAR_GETPTR_H
#define MIDDLEEND_MLIR_DIALECT_VAR_GETPTR_H

#include <cstddef>
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/var/basic.h"
namespace middleend::mlir::var {

class VarGetPtrOp : public VarOps {
 public:
  static auto Create(context::MLIRContext *context, Value *ptr, std::vector<Value *> indices = {}) -> VarGetPtrOp * {
    auto op = new VarGetPtrOp();
    op->SetUp(context, ptr, std::move(indices));
    return op;
  }

  /**
   * @brief Since the operation of getting a variable can be
   * accessing an array element, thus it will need a list of indices to
   * specify which element to access, and the result of this operation will be the res_
   */
  auto GetIndices() -> std::vector<Value *> {
    auto indices = std::vector<Value *>{};
    for (size_t operand_index = 1; operand_index < operands_.size(); operand_index++) {
      indices.emplace_back(operands_[operand_index]);
    }
    return indices;
  }
  auto GetPtr() -> Value * { return operands_[0]; }
  auto GetRes() -> Value * { return results_[0]; }
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

 private:
  VarGetPtrOp() : VarOps("getptr"){};
  auto SetUp(context::MLIRContext *context, Value *ptr, std::vector<Value *> indices = {}) -> void {
    operands_.reserve(indices.size() + 1);
    results_.reserve(1);
    operands_.emplace_back(ptr);
    operands_.insert(operands_.end(), indices.begin(), indices.end());
    auto res = context->CreateValue();
    results_.emplace_back(res);
    res->op_ = this;
  }
};
}  // namespace middleend::mlir::var

#endif