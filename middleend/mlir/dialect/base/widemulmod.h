#ifndef MIDDLEEND_MLIR_DIALECT_BASE_WIDEMULMOD_H
#define MIDDLEEND_MLIR_DIALECT_BASE_WIDEMULMOD_H

#include "middleend/mlir/dialect/base/basic.h"
#include "middleend/mlir/utils/value/value.h"

namespace middleend::mlir::base {

class WideMulModOp : public BaseOps {
 public:
  static auto Create(mlir::Value *result, mlir::Value *lhs, mlir::Value *rhs, mlir::Value *modulus) -> WideMulModOp * {
    auto *op = new WideMulModOp();
    op->operands_ = {lhs, rhs, modulus};
    op->results_ = {result};
    result->op_ = op;
    return op;
  }
  auto GetLHS() -> mlir::Value * { return operands_[0]; }
  auto GetRHS() -> mlir::Value * { return operands_[1]; }
  auto GetModulus() -> mlir::Value * { return operands_[2]; }
  auto GetRes() -> mlir::Value * { return results_[0]; }
  auto Emit(std::ofstream *out) -> void override;
  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  WideMulModOp() : BaseOps("wide_mul_mod") {}
};

}  // namespace middleend::mlir::base

#endif
