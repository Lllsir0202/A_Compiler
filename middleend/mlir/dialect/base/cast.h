#ifndef MIDDLEEND_MLIR_DIALECT_BASE_CAST_H
#define MIDDLEEND_MLIR_DIALECT_BASE_CAST_H
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/basic.h"
namespace middleend::mlir::base {
// Since the structure of the cast operations is similar,
// we can use a macro to define these cast operations
#include "middleend/mlir/dialect/base/def/cast.def"

DEFINE_CAST_OP(Int2Float, int2float, INT, FLOAT)
DEFINE_CAST_OP(Float2Int, float2int, FLOAT, INT)
DEFINE_CAST_OP(Bool2Int, bool2int, BOOL, INT)
DEFINE_CAST_OP(Int2Bool, int2bool, INT, BOOL)
DEFINE_CAST_OP(Bool2Float, bool2float, BOOL, FLOAT)
DEFINE_CAST_OP(Float2Bool, float2bool, FLOAT, BOOL)

}  // namespace middleend::mlir::base

#endif