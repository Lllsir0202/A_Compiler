#ifndef MIDDLEEND_MLIR_DIALECT_ARITHM_WIDEMULMOD_H
#define MIDDLEEND_MLIR_DIALECT_ARITHM_WIDEMULMOD_H

#include "context/mlir_context.h"
#include "middleend/mlir/dialect/arithm/basic.h"

namespace middleend::mlir::arithm {

class WideMulModOp : public ArithmOps {
 public:
  static auto Create(context::MLIRContext *context, mlir::Value *lhs, mlir::Value *rhs,
                     mlir::Value *modulus) -> WideMulModOp * {
    auto *op = new WideMulModOp();
    op->operands_ = {lhs, rhs, modulus};
    auto *result = context->CreateValue();
    result->op_ = op;
    op->results_ = {result};
    return op;
  }
  auto GetLHS() -> mlir::Value * { return operands_[0]; }
  auto GetRHS() -> mlir::Value * { return operands_[1]; }
  auto GetModulus() -> mlir::Value * { return operands_[2]; }
  auto GetRes() -> mlir::Value * { return results_[0]; }
  auto Emit(std::ofstream *out) -> void override;
  auto Verify(context::CompilerContext *context) -> bool override;

 private:
  WideMulModOp() : ArithmOps("wide_mul_mod") {}
};

}  // namespace middleend::mlir::arithm

#endif
