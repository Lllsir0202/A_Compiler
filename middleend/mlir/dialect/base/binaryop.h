#ifndef MIDDLEEND_MLIR_DIALECT_BASE_BINARYOP_H
#define MIDDLEEND_MLIR_DIALECT_BASE_BINARYOP_H
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/basic.h"
#include "mlir/utils/value/value.h"
namespace middleend::mlir::base {

#include "middleend/mlir/dialect/base/def/intbinaryop.def"

DEFINE_INTBINARY_OP(Add, add)
DEFINE_INTBINARY_OP(Sub, sub)
DEFINE_INTBINARY_OP(Mul, mul)
DEFINE_INTBINARY_OP(Div, div)
DEFINE_INTBINARY_OP(Mod, mod)
DEFINE_INTBINARY_OP(Shl, shl)
DEFINE_INTBINARY_OP(LShr, lshr)
DEFINE_INTBINARY_OP(AShr, ashr)
DEFINE_INTBINARY_OP(Equal, equal)
DEFINE_INTBINARY_OP(NotEqual, ne)
DEFINE_INTBINARY_OP(Less, less)
DEFINE_INTBINARY_OP(LessEqual, le)
DEFINE_INTBINARY_OP(Greater, greater)
DEFINE_INTBINARY_OP(GreaterEqual, ge)

#include "middleend/mlir/dialect/base/def/floatbinaryop.def"

DEFINE_FLOATBINARY_OP(AddF, addf)
DEFINE_FLOATBINARY_OP(SubF, subf)
DEFINE_FLOATBINARY_OP(MulF, mulf)
DEFINE_FLOATBINARY_OP(DivF, divf)
// ModF is not allowed.
// DEFINE_FLOATBINARY_OP(ModF, modf)
DEFINE_FLOATBINARY_OP(EqualF, equalf)
DEFINE_FLOATBINARY_OP(NotEqualF, neF)
DEFINE_FLOATBINARY_OP(LessF, lessf)
DEFINE_FLOATBINARY_OP(LessEqualF, lef)
DEFINE_FLOATBINARY_OP(GreaterF, greaterf)
DEFINE_FLOATBINARY_OP(GreaterEqualF, gef)

}  // namespace middleend::mlir::base

#endif
