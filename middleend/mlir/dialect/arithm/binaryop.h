#ifndef MIDDLEEND_MLIR_DIALECT_ARITHM_BINARYOP_H
#define MIDDLEEND_MLIR_DIALECT_ARITHM_BINARYOP_H
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/basic.h"
#include "middleend/mlir/utils/value/value.h"
namespace middleend::mlir::arithm {

#include "middleend/mlir/dialect/arithm/binaryop.def"

DEFINE_BINARY_OP(Add, add)
DEFINE_BINARY_OP(Sub, sub)
DEFINE_BINARY_OP(Mul, mul)
DEFINE_BINARY_OP(Div, div)
DEFINE_BINARY_OP(Mod, mod)
DEFINE_BINARY_OP(Equal, equal)
DEFINE_BINARY_OP(NotEqual, ne)
DEFINE_BINARY_OP(Less, less)
DEFINE_BINARY_OP(LessEqual, le)
DEFINE_BINARY_OP(Greater, greater)
DEFINE_BINARY_OP(GreaterEqual, ge)

/**
 * @note Since we need to consider the short-circuiting behavior of the logical operations,
 * we should use ifOp to implement the logical operations,
 * instead of directly using the logical operation
 */
// DEFINE_BINARY_OP(And)
// DEFINE_BINARY_OP(Or)
}  // namespace middleend::mlir::arithm
#endif