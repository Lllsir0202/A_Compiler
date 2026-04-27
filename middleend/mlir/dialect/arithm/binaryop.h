#ifndef MIDDLEEND_MLIR_DIALECT_ARITHM_BINARYOP_H
#define MIDDLEEND_MLIR_DIALECT_ARITHM_BINARYOP_H
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/arithm/basic.h"
#include "middleend/mlir/utils/value/value.h"
namespace middleend::mlir::arithm {

#include "middleend/mlir/dialect/arithm/binaryop.def"

DEFINE_BINARY_OP(Add)
DEFINE_BINARY_OP(Sub)
DEFINE_BINARY_OP(Mul)
DEFINE_BINARY_OP(Div)
DEFINE_BINARY_OP(Mod)
DEFINE_BINARY_OP(Equal)
DEFINE_BINARY_OP(NotEqual)
DEFINE_BINARY_OP(Less)
DEFINE_BINARY_OP(LessEqual)
DEFINE_BINARY_OP(Greater)
DEFINE_BINARY_OP(GreaterEqual)
DEFINE_BINARY_OP(And)
DEFINE_BINARY_OP(Or)
}  // namespace middleend::mlir::arithm
#endif